#include "tunnel.hpp"
#include <llarp/service/convotag.hpp>
#include <llarp/service/endpoint.hpp>
#include <llarp/service/name.hpp>
#include "llarp/net/net_int.hpp"
#include "stream.hpp"
#include <limits>
#include <llarp/util/logging.hpp>
#include <llarp/util/logging/buffer.hpp>
#include <llarp/util/str.hpp>
#include <llarp/ev/libuv.hpp>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace llarp::quic
{
  namespace
  {
    static auto logcat = log::Cat("quic");

    // Takes data from the tcp connection and pushes it down the quic tunnel
    void
    on_outgoing_data(uvw::DataEvent& event, uvw::TCPHandle& client)
    {
      auto stream = client.data<Stream>();
      assert(stream);
      std::string_view data{event.data.get(), event.length};
      auto peer = client.peer();
      // log::trace(logcat, "{}:{} → lokinet {}", peer.ip, peer.port, buffer_printer{data});
      log::debug(logcat, "{}:{} → lokinet {}", peer.ip, peer.port, buffer_printer{data});
      // Steal the buffer from the DataEvent's unique_ptr<char[]>:
      stream->append_buffer(reinterpret_cast<const std::byte*>(event.data.release()), event.length);
      if (stream->used() >= tunnel::PAUSE_SIZE)
      {
        log::debug(
            logcat,
            "quic tunnel is congested (have {} bytes in flight); pausing local tcp connection "
            "reads",
            stream->used());
        client.stop();
        stream->when_available([](Stream& s) {
          auto client = s.data<uvw::TCPHandle>();
          if (s.used() < tunnel::PAUSE_SIZE)
          {
            log::debug(
                logcat, "quic tunnel is no longer congested; resuming tcp connection reading");
            client->read();
            return true;
          }
          return false;
        });
      }
      else
      {
        log::debug(logcat, "Queued {} bytes", event.length);
      }
    }

    // Received data from the quic tunnel and sends it to the TCP connection
    void
    on_incoming_data(Stream& stream, bstring_view bdata)
    {
      auto tcp = stream.data<uvw::TCPHandle>();
      if (!tcp)
        return;  // TCP connection is gone, which would have already sent a stream close, so just
                 // drop it.

      std::string_view data{reinterpret_cast<const char*>(bdata.data()), bdata.size()};
      auto peer = tcp->peer();
      log::trace(logcat, "{}:{} ← lokinet {}", peer.ip, peer.port, buffer_printer{data});

      if (data.empty())
        return;

      // Try first to write immediately from the existing buffer to avoid needing an
      // allocation and copy:
      auto written = tcp->tryWrite(const_cast<char*>(data.data()), data.size());
      if (written < (int)data.size())
      {
        data.remove_prefix(written);

        auto wdata = std::make_unique<char[]>(data.size());
        std::copy(data.begin(), data.end(), wdata.get());
        tcp->write(std::move(wdata), data.size());
      }
    }

    void
    close_tcp_pair(quic::Stream& st, std::optional<uint64_t> /*errcode*/)
    {
      if (auto tcp = st.data<uvw::TCPHandle>())
      {
        log::trace(logcat, "Closing TCP connection");
        tcp->close();
      }
    };
    // Creates a new tcp handle that forwards incoming data/errors/closes into appropriate actions
    // on the given quic stream.
    void
    install_stream_forwarding(uvw::TCPHandle& tcp, Stream& stream)
    {
      tcp.data(stream.shared_from_this());
      stream.weak_data(tcp.weak_from_this());
      auto weak_conn = stream.get_connection().weak_from_this();

      tcp.clear();  // Clear any existing initial event handlers

      tcp.on<uvw::CloseEvent>([weak_conn = std::move(weak_conn)](auto&, uvw::TCPHandle& c) {
        // This fires sometime after we call `close()` to signal that the close is done.
        if (auto stream = c.data<Stream>())
        {
          log::info(
              logcat,
              "Local TCP connection closed, closing associated quic stream {}",
              stream->id());

          // There is an awkwardness with Stream ownership, so make sure the Connection
          // which it holds a reference to still exists, as stream->close will segfault
          // otherwise
          if (auto locked_conn = weak_conn.lock())
          {
            // If this end of the stream closed due to an abrupt close to the local TCP
            // connection rather than an EOF, send an error code along so the other end
            // knows this end is dead.  Otherwise, the streams on either end should die
            // gracefully when both ends of the TCP connection are properly closed.
            stream->close(stream->has_eof() ? 0 : tunnel::ERROR_TCP);
          }
          stream->data(nullptr);
        }
        c.data(nullptr);
      });
      tcp.on<uvw::EndEvent>([](auto&, uvw::TCPHandle& c) {
        // This fires on eof, most likely because the other side of the TCP connection closed it.
        log::info(logcat, "EOF on connection to {}:{}", c.peer().ip, c.peer().port);
        if (auto stream = c.data<Stream>())
        {
          stream->set_eof();  // CloseEvent will send graceful shutdown to other end
        }
        c.close();
      });
      tcp.on<uvw::ErrorEvent>([](const uvw::ErrorEvent& e, uvw::TCPHandle& tcp) {
        log::error(
            logcat,
            "ErrorEvent[{}:{}] on connection with {}:{}, shutting down quic stream",
            e.name(),
            e.what(),
            tcp.peer().ip,
            tcp.peer().port);
        if (auto stream = tcp.data<Stream>())
        {
          stream->close(tunnel::ERROR_TCP);
          stream->data(nullptr);
          tcp.data(nullptr);
        }
        tcp.close();
      });
      tcp.on<uvw::DataEvent>(on_outgoing_data);
      stream.data_callback = on_incoming_data;
      stream.close_callback = close_tcp_pair;
    }
    // This initial data handler is responsible for pulling off the initial stream data that comes
    // back, confirming that the tunnel is opened on the other end.  Currently this is a null byte
    // (CONNECT_INIT) but in the future we might encode additional data here (and, if that happens,
    // we want this older implementation to fail).
    //
    // If the initial byte checks out we replace this handler with the regular stream handler (and
    // forward the rest of the data to it if we got more than just the single byte).
    void
    initial_client_data_handler(uvw::TCPHandle& client, Stream& stream, bstring_view bdata)
    {
      log::trace(logcat, "initial client handler; data: {}", buffer_printer{bdata});
      if (bdata.empty())
        return;
      client.clear();  // Clear these initial event handlers: we either set up the proper ones, or
                       // close

      if (auto b0 = bdata[0]; b0 == tunnel::CONNECT_INIT)
      {
        // Set up callbacks, which replaces both of these initial callbacks
        client.read();  // Unfreeze (we stop() before putting into pending)
        install_stream_forwarding(client, stream);

        if (bdata.size() > 1)
        {
          bdata.remove_prefix(1);
          stream.data_callback(stream, std::move(bdata));
        }
        log::trace(logcat, "starting client reading");
      }
      else
      {
        log::warning(
            logcat,
            "Remote connection returned invalid initial byte (0x{}); dropping "
            "connection",
            oxenc::to_hex(bdata.begin(), bdata.begin() + 1));
        stream.close(tunnel::ERROR_BAD_INIT);
        client.close();
      }
      stream.io_ready();
    }

    // Initial close handler that gets replaced as soon as we receive a valid byte (in the above
    // handler).  If this gets called then it means the quic remote quic end closed before we
    // established the end-to-end tunnel (for example because the remote's tunnel connection
    // failed):
    void
    initial_client_close_handler(
        uvw::TCPHandle& client, Stream& /*stream*/, std::optional<uint64_t> error_code)
    {
      if (error_code && *error_code == tunnel::ERROR_CONNECT)
        log::debug(logcat, "Remote TCP connection failed, closing local connection");
      else
        log::warning(
            logcat,
            "Stream connection closed {}; closing local TCP connection.",
            error_code ? "with error " + std::to_string(*error_code) : "gracefully");
      auto peer = client.peer();
      log::debug(logcat, "Closing connection to {}:{}", peer.ip, peer.port);
      client.clear();
      if (error_code)
        client.close();
      else
        client.close();
    }

  }  // namespace

  TunnelManager::TunnelManager(EndpointBase& se) : service_endpoint_{se}
  {
    // Cleanup callback to clear out closed tunnel connections
    service_endpoint_.Loop()->call_every(500ms, timer_keepalive_, [this] {
      log::trace(logcat, "Checking quic tunnels for finished connections");
      for (auto ctit = client_tunnels_.begin(); ctit != client_tunnels_.end();)
      {
        // Clear any accepted connections that have been closed:
        auto& [port, ct] = *ctit;
        for (auto it = ct.conns.begin(); it != ct.conns.end();)
        {
          // TCP connections keep a shared_ptr to their quic::Stream while open and clear it when
          // closed.  (We don't want to use `.active()` here because we do deliberately temporarily
          // stop the TCP connection when the quic side gets congested.
          if (not *it or not(*it)->data())
          {
            log::debug(logcat, "Cleanup up closed outgoing tunnel on quic:{}", port);
            it = ct.conns.erase(it);
          }
          else
            ++it;
        }

        // If there are not accepted connections left *and* we stopped listening for new ones then
        // destroy the whole thing.
        if (ct.conns.empty() and (not ct.tcp or not ct.tcp->active()))
        {
          log::debug(logcat, "All sockets closed on quic:{}, destroying tunnel data", port);
          if (ct.close_cb)
            ct.close_cb(0, nullptr);
          ctit = client_tunnels_.erase(ctit);
        }
        else
          ++ctit;
      }
      log::trace(logcat, "Done quic tunnel cleanup check");
    });
  }

  void
  TunnelManager::make_server()
  {
    server_ = std::make_unique<Server>(service_endpoint_);
    server_->stream_open_callback = [this](Stream& stream, uint16_t port) -> bool {
      stream.close_callback = close_tcp_pair;

      // FIXME
      auto& conn = stream.get_connection();

      auto lokinet_addr =
          var::visit([](auto&& remote) { return remote.ToString(); }, *conn.path.remote.endpoint);
      auto tunnel_to = allow_connection(lokinet_addr, port);
      if (not tunnel_to)
        return false;
      log::debug(
          logcat, "quic stream from {} to {} tunnelling to {}", lokinet_addr, port, *tunnel_to);

      auto tcp = get_loop()->resource<uvw::TCPHandle>();
      auto error_handler = tcp->once<uvw::ErrorEvent>(
          [&stream, to = *tunnel_to](const uvw::ErrorEvent&, uvw::TCPHandle&) {
            log::warning(logcat, "Failed to connect to {}, shutting down quic stream", to);
            stream.close(tunnel::ERROR_CONNECT);
          });

      // As soon as we connect to the local tcp tunnel port we fire a CONNECT_INIT down the stream
      // tunnel to let the other end know the connection was successful, then set up regular
      // stream handling to handle any other to/from data.
      tcp->once<uvw::ConnectEvent>(
          [streamw = stream.weak_from_this(), error_handler = std::move(error_handler)](
              const uvw::ConnectEvent&, uvw::TCPHandle& tcp) {
            auto peer = tcp.peer();
            auto stream = streamw.lock();
            if (!stream)
            {
              log::warning(
                  logcat,
                  "Connected to TCP {}:{} but quic stream has gone away; close/resetting local TCP "
                  "connection",
                  peer.ip,
                  peer.port);
              tcp.close();
              return;
            }
            log::debug(logcat, "Connected to {}:{} for quic {}", peer.ip, peer.port, stream->id());
            // Set up the data stream forwarding (which also clears these initial handlers).
            install_stream_forwarding(tcp, *stream);
            assert(stream->used() == 0);

            // Send the magic byte, and start reading from the tcp tunnel in the logic thread
            stream->append_buffer(new std::byte[1]{tunnel::CONNECT_INIT}, 1);
            tcp.read();
          });

      tcp->connect(*tunnel_to->operator const sockaddr*());

      return true;
    };
  }

  int
  TunnelManager::listen(ListenHandler handler)
  {
    if (!handler)
      throw std::logic_error{"Cannot call listen() with a null handler"};
    assert(service_endpoint_.Loop()->inEventLoop());
    if (not server_)
      make_server();

    int id = next_handler_id_++;
    incoming_handlers_.emplace_hint(incoming_handlers_.end(), id, std::move(handler));
    return id;
  }

  int
  TunnelManager::listen(SockAddr addr)
  {
    return listen([addr](std::string_view, uint16_t p) -> std::optional<SockAddr> {
      log::info(logcat, "try accepting {}", addr.getPort());
      if (p == addr.getPort())
        return addr;
      return std::nullopt;
    });
  }

  void
  TunnelManager::forget(int id)
  {
    incoming_handlers_.erase(id);
  }

  std::optional<SockAddr>
  TunnelManager::allow_connection(std::string_view lokinet_addr, uint16_t port)
  {
    for (auto& [id, handler] : incoming_handlers_)
    {
      try
      {
        if (auto addr = handler(lokinet_addr, port))
          return addr;
      }
      catch (const std::exception& e)
      {
        log::warning(
            logcat,
            "Incoming quic connection from {} to {} denied via exception ({})",
            lokinet_addr,
            port,
            e.what());
        return std::nullopt;
      }
    }
    log::warning(
        logcat,
        "Incoming quic connection from {} to {} declined by all handlers",
        lokinet_addr,
        port);
    return std::nullopt;
  }

  std::shared_ptr<uvw::Loop>
  TunnelManager::get_loop()
  {
    if (auto loop = service_endpoint_.Loop()->MaybeGetUVWLoop())
      return loop;
    throw std::logic_error{"TunnelManager requires a libuv-based event loop"};
  }

  // Finds the first unused key in `map`, starting at `start` and wrapping back to 0 if we hit the
  // end.  Requires an unsigned int type for the key.  Requires nullopt if the map is completely
  // full, otherwise returns the free key.
  template <
      typename K,
      typename V,
      typename = std::enable_if_t<std::is_integral_v<K> && std::is_unsigned_v<K>>>
  static std::optional<K>
  find_unused_key(std::map<K, V>& map, K start)
  {
    if (map.size() == std::numeric_limits<K>::max())
      return std::nullopt;  // The map is completely full
    [[maybe_unused]] bool from_zero = (start == K{0});

    // Start at the first key >= start, then walk 1-by-1 (incrementing start) until we find a
    // strictly > key, which means we've found a hole we can use
    auto it = map.lower_bound(start);
    if (it == map.end())
      return start;

    for (; it != map.end(); ++it, ++start)
      if (it->first != start)
        return start;
    if (start != 0)  // `start` didn't wrap which means we found an empty slot
      return start;
    assert(!from_zero);  // There *must* be a free slot somewhere in [0, max] (otherwise the map
                         // would be completely full and we'd have returned nullopt).
    return find_unused_key(map, K{0});
  }

  // Wrap common tasks and cleanup that we need to do from multiple places while establishing a
  // tunnel
  bool
  TunnelManager::continue_connecting(
      uint16_t pseudo_port, bool step_success, std::string_view step_name, std::string_view addr)
  {
    assert(service_endpoint_.Loop()->inEventLoop());
    auto it = client_tunnels_.find(pseudo_port);
    if (it == client_tunnels_.end())
    {
      log::debug(logcat, "QUIC tunnel to {} closed before {} finished", addr, step_name);
      return false;
    }
    if (!step_success)
    {
      log::warning(logcat, "QUIC tunnel to {} failed during {}; aborting tunnel", addr, step_name);
      it->second.tcp->close();
      if (it->second.open_cb)
        it->second.open_cb(false, nullptr);
      client_tunnels_.erase(it);
    }
    return step_success;
  }

  std::pair<SockAddr, uint16_t>
  TunnelManager::open(
      std::string_view remote_address,
      uint16_t port,
      OpenCallback on_open,
      CloseCallback on_close,
      SockAddr bind_addr)
  {
    std::string remote_addr = lowercase_ascii_string(std::string{remote_address});

    std::pair<SockAddr, uint16_t> result;
    auto& [saddr, pport] = result;

    auto maybe_remote = service::ParseAddress(remote_addr);
    if (!maybe_remote)
    {
      if (not service::NameIsValid(remote_addr))
        throw std::invalid_argument{"Invalid remote lokinet name/address"};
      // Otherwise it's a valid ONS name, so we'll initiate an ONS lookup below
    }

    // Open the TCP tunnel right away; it will just block new incoming connections until the quic
    // connection is established, but this still allows the caller to connect right away and queue
    // an initial request (rather than having to wait via a callback before connecting).  It also
    // makes sure we can actually listen on the given address before we go ahead with establishing
    // the quic connection.
    auto tcp_tunnel = get_loop()->resource<uvw::TCPHandle>();
    const char* failed = nullptr;
    auto err_handler =
        tcp_tunnel->once<uvw::ErrorEvent>([&failed](auto& evt, auto&) { failed = evt.what(); });
    tcp_tunnel->bind(*bind_addr.operator const sockaddr*());
    tcp_tunnel->on<uvw::ListenEvent>([this](const uvw::ListenEvent&, uvw::TCPHandle& tcp_tunnel) {
      auto client = tcp_tunnel.loop().resource<uvw::TCPHandle>();
      tcp_tunnel.accept(*client);
      // Freeze the connection (after accepting) because we may need to stall it until a stream
      // becomes available; flush_pending_incoming will unfreeze it.
      client->stop();
      auto pport = tcp_tunnel.data<uint16_t>();
      if (pport)
      {
        if (auto it = client_tunnels_.find(*pport); it != client_tunnels_.end())
        {
          it->second.pending_incoming.emplace(std::move(client));
          flush_pending_incoming(it->second);
          return;
        }
        tcp_tunnel.data(nullptr);
      }
      client->close();
    });
    tcp_tunnel->listen();
    tcp_tunnel->erase(err_handler);

    if (failed)
    {
      tcp_tunnel->close();
      throw std::runtime_error{fmt::format(
          "Failed to bind/listen local TCP tunnel socket on {}: {}", bind_addr, failed)};
    }

    auto bound = tcp_tunnel->sock();
    saddr = SockAddr{bound.ip, huint16_t{static_cast<uint16_t>(bound.port)}};

    // Find the first unused psuedo-port value starting from next_pseudo_port_.
    if (auto p = find_unused_key(client_tunnels_, next_pseudo_port_))
      pport = *p;
    else
      throw std::runtime_error{
          "Unable to open an outgoing quic connection: too many existing connections"};
    (next_pseudo_port_ = pport)++;

    log::trace(logcat, "Bound TCP tunnel {} for quic client :{}", saddr, pport);

    // We are emplacing into client_tunnels_ here: beyond this point we must not throw until we
    // return (or if we do, make sure we remove this row from client_tunnels_ first).
    assert(client_tunnels_.count(pport) == 0);
    auto& ct = client_tunnels_[pport];
    ct.open_cb = std::move(on_open);
    ct.close_cb = std::move(on_close);
    ct.tcp = std::move(tcp_tunnel);
    // We use this pport shared_ptr value on the listening tcp socket both to hand to pport into the
    // accept handler, and to let the accept handler know that `this` is still safe to use.
    ct.tcp->data(std::make_shared<uint16_t>(pport));

    auto after_path = [this, port, pport = pport, remote_addr](auto maybe_addr) {
      if (maybe_addr)
      {
        make_client(port, *maybe_addr, *client_tunnels_.find(pport));
        return;
      }
      continue_connecting(pport, false, "path build", remote_addr);
    };

    if (!maybe_remote)
    {
      // We were given an ONS address, so it's a two-step process: first we resolve the ONS name,
      // then we have to build a path to that address.
      service_endpoint_.LookupNameAsync(
          remote_addr,
          [this, after_path = std::move(after_path), pport = pport, remote_addr](
              auto maybe_remote) {
            if (not continue_connecting(
                    pport, (bool)maybe_remote, "endpoint ONS lookup", remote_addr))
              return;
            service_endpoint_.MarkAddressOutbound(*maybe_remote);
            service_endpoint_.EnsurePathTo(*maybe_remote, after_path, open_timeout);
          });
      return result;
    }

    auto& remote = *maybe_remote;

    // See if we have an existing convo tag we can use to start things immediately
    if (auto maybe_convo = service_endpoint_.GetBestConvoTagFor(remote);
        auto maybe_addr = service_endpoint_.GetEndpointWithConvoTag(*maybe_convo))
      after_path(maybe_addr);
    else
    {
      service_endpoint_.MarkAddressOutbound(remote);
      service_endpoint_.EnsurePathTo(remote, after_path, open_timeout);
    }

    return result;
  }

  void
  TunnelManager::close(int id)
  {
    if (auto it = client_tunnels_.find(id); it != client_tunnels_.end())
    {
      it->second.tcp->close();
      it->second.tcp->data(nullptr);
      it->second.tcp.reset();
    }
  }

  TunnelManager::ClientTunnel::~ClientTunnel()
  {
    if (tcp)
    {
      tcp->close();
      tcp->data(nullptr);
      tcp.reset();
    }
    for (auto& conn : conns)
      conn->close();
    conns.clear();

    while (not pending_incoming.empty())
    {
      if (auto tcp = pending_incoming.front().lock())
      {
        tcp->clear();
        tcp->close();
      }
      pending_incoming.pop();
    }
  }

  void
  TunnelManager::make_client(
      const uint16_t port,
      std::variant<service::Address, RouterID> remote,
      std::pair<const uint16_t, ClientTunnel>& row)
  {
    assert(port > 0);
    auto& [pport, tunnel] = row;
    assert(not tunnel.client);
    tunnel.client = std::make_unique<Client>(service_endpoint_, port, std::move(remote), pport);
    auto conn = tunnel.client->get_connection();

    conn->on_stream_available = [this, id = row.first](Connection&) {
      log::debug(logcat, "QUIC connection :{} established; streams now available", id);
      if (auto it = client_tunnels_.find(id); it != client_tunnels_.end())
      {
        flush_pending_incoming(it->second);
        if (it->second.open_cb)
        {
          log::trace(logcat, "Calling ClientTunnel.open_cb()");
          it->second.open_cb(true, nullptr);
          it->second.open_cb = nullptr;  // only call once
        }
      }
      else
        log::warning(
            logcat, "Connection.on_stream_available fired but we have no associated ClientTunnel!");
    };
    conn->on_closing = [this, id = row.first](Connection&) {
      log::debug(logcat, "QUIC connection :{} closing, closing tunnel", id);
      if (auto it = client_tunnels_.find(id); it != client_tunnels_.end())
      {
        if (it->second.close_cb)
        {
          log::trace(logcat, "Calling ClientTunnel.close_cb()");
          it->second.close_cb(0, nullptr);
        }
      }
      else
        log::debug(logcat, "Connection.on_closing fired but no associated ClientTunnel found.");

      this->close(id);
    };
  }

  void
  TunnelManager::flush_pending_incoming(ClientTunnel& ct)
  {
    if (!ct.client)
      return;  // Happens if we're still waiting for a path to build
    if (not ct.client->get_connection())
      return;
    auto& conn = *ct.client->get_connection();
    int available = conn.get_streams_available();
    while (available > 0 and not ct.pending_incoming.empty())
    {
      auto tcp_client = ct.pending_incoming.front().lock();
      ct.pending_incoming.pop();
      if (not tcp_client)
        continue;

      try
      {
        auto str = conn.open_stream(
            [tcp_client](auto&&... args) {
              initial_client_data_handler(*tcp_client, std::forward<decltype(args)>(args)...);
            },
            [tcp_client](auto&&... args) {
              initial_client_close_handler(*tcp_client, std::forward<decltype(args)>(args)...);
            });
        available--;
      }
      catch (const std::exception& e)
      {
        log::warning(logcat, "Opening quic stream failed: {}", e.what());
        tcp_client->close();
      }

      log::trace(logcat, "Set up new stream");
      conn.io_ready();
    }
  }

  void
  TunnelManager::receive_packet(
      std::variant<service::Address, RouterID> remote, const llarp_buffer_t& buf)
  {
    if (buf.sz <= 4)
    {
      log::warning(logcat, "invalid quic packet: packet size ({}) too small", buf.sz);
      return;
    }
    auto type = static_cast<std::byte>(buf.base[0]);
    nuint16_t pseudo_port_n;
    std::memcpy(&pseudo_port_n.n, &buf.base[1], 2);
    uint16_t pseudo_port = ToHost(pseudo_port_n).h;
    auto ecn = static_cast<uint8_t>(buf.base[3]);
    bstring_view data{reinterpret_cast<const std::byte*>(&buf.base[4]), buf.sz - 4};

    huint16_t remote_port{pseudo_port};

    quic::Endpoint* ep = nullptr;

    if (type == CLIENT_TO_SERVER)
    {
      log::debug(logcat, "packet is client-to-server from client pport {}", pseudo_port);

      if (!server_)
      {
        log::warning(logcat, "Dropping incoming quic packet to server: no listeners");
        return;
      }
      ep = server_.get();
    }
    else if (type == SERVER_TO_CLIENT)
    {
      log::debug(logcat, "packet is server-to-client to client pport {}", pseudo_port);
      // Server-to-client: the header port tells us which client tunnel this is going to
      if (auto it = client_tunnels_.find(pseudo_port); it != client_tunnels_.end())
        ep = it->second.client.get();

      if (not ep)
      {
        log::warning(logcat, "Incoming quic packet to invalid/closed client; dropping");
        return;
      }

      // The server doesn't send back the port because we already know it 1-to-1 from our outgoing
      // connection
      if (auto conn = static_cast<quic::Client&>(*ep).get_connection())
      {
        remote_port = llarp::net::ToHost(conn->path.remote.port());
        log::debug(logcat, "remote port is {}", remote_port);
      }
      else
      {
        log::warning(
            logcat, "Incoming quic to a quic::Client without an active quic::Connection; dropping");
        return;
      }
    }
    else
    {
      log::warning(logcat, "Invalid incoming quic packet type {}; dropping packet", type);
      return;
    }

    log::debug(
        logcat,
        "remote_port = {}, pseudo_port = {} at line {}",
        remote_port,
        pseudo_port,
        __LINE__);

    auto remote_addr = Address{SockAddr{"::1"sv, remote_port}, std::move(remote)};
    ep->receive_packet(std::move(remote_addr), ecn, data);
  }
}  // namespace llarp::quic
