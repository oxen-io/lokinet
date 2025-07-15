#include "session.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/handlers/session.hpp>
#include <llarp/handlers/tun.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/messages/path.hpp>
#include <llarp/messages/session.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/bspan.hpp>
#include <llarp/util/formattable.hpp>

#include <oxen/quic/context.hpp>
#include <oxen/quic/gnutls_crypto.hpp>
#include <oxen/quic/udp.hpp>
#include <oxenc/hex.h>

#include <utility>

namespace
{
    using namespace oxenc::literals;
    // GNUTLS Creds tunnel default keys until we implement null-crypto in libquic
    inline constexpr auto TUNNEL_SEED = "0000000000000000000000000000000000000000000000000000000000000000"_hex;
    inline constexpr auto TUNNEL_PUBKEY = "3b6a27bcceb6a42d62a3a8d02a6f0d73653215771de243a63ac048a18b59da29"_hex;
}  // anonymous namespace

namespace llarp::session
{
    namespace quic = oxen::quic;

    static auto logcat = log::Cat("session");
    struct TCPTunnel
    {
        const quic::Address FAKE_QUIC_ADDR{"127.86.75.30"s, 9};
        const quic::Path FAKE_QUIC_PATH{FAKE_QUIC_ADDR, FAKE_QUIC_ADDR};

        std::shared_ptr<quic::GNUTLSCreds> tls_creds = quic::GNUTLSCreds::make_from_ed_keys(TUNNEL_SEED, TUNNEL_PUBKEY);

        std::shared_ptr<quic::Endpoint> quic_ep{nullptr};
        std::shared_ptr<quic::Connection> quic_conn{nullptr};

        // (session initiator) TCPHandle listeners mapped to the destination port they are mapped for
        std::unordered_map<uint16_t, std::shared_ptr<TCPHandle>> tcp_handles;

        // (session remote) QUIC stream ID to TCP connection
        std::vector<std::shared_ptr<TCPConnection>> _tcp_conns;

        BaseSession& session;

        std::shared_ptr<bool> destructor_canary{std::make_shared<bool>(true)};

        ~TCPTunnel() { reset(); }

        // the QUIC endpoint should be fine if the QUIC connection closes, but
        // TCP conns and port mappings will need to be restarted.
        void reset()
        {
            log::critical(logcat, "TCPTunnel::reset()");
            quic_conn.reset();
            _tcp_conns.clear();
            tcp_handles.clear();
            log::critical(logcat, "TCPTunnel::reset() END");
        }

        TCPTunnel(BaseSession& _session) : session(_session)
        {
            quic::opt::manual_routing quic_send{[this](const quic::Path&, std::span<const std::byte> data) {
                session.send_path_data_message(data, traffic_type::TUNNELED_QUIC);
            }};
            quic::connection_established_callback new_conn{[this](quic::Connection& conn) {
                if (quic_conn)
                {
                    log::error(logcat, "Already have connection for QUIC tunnel for session to {}!", session._remote);
                    return;
                }
                log::debug(logcat, "New connection for QUIC tunnel for session to {}!", session._remote);
                quic_conn = conn.shared_from_this();
            }};
            quic::connection_closed_callback conn_closed{
                [this, canary = std::weak_ptr{destructor_canary}](quic::Connection&, uint64_t) {
                    if (!quic_conn)
                    {
                        log::warning(
                            logcat,
                            "Received conn closed, but this session's QUIC tunnel does not seem to have an open "
                            "connection, remote: {}",
                            session._remote);
                        return;
                    }
                    log::debug(logcat, "QUIC TCP tunnel conn to {} closed.", session._remote);

                    // this could fire from quic::Endpoint destructor, at which point
                    // the members `reset` would reset may no longer be valid objects
                    if (canary.lock())
                        reset();
                }};

            auto stream_opened = [this](quic::Stream& stream) {
                stream.set_stream_data_cb([this, prev_byte = std::optional<std::byte>{std::nullopt}](
                                              quic::Stream& stream, std::span<const std::byte> data) mutable {
                    uint16_t dest_port{0};

                    if (data.empty())
                    {
                        log::error(logcat, "QUIC stream data callback with no data!");
                        return;
                    }
                    if (prev_byte)
                    {
                        std::array<std::byte, 2> buf;
                        buf[0] = *prev_byte;
                        buf[1] = data[0];
                        dest_port = oxenc::load_big_to_host<uint16_t>(buf.data());
                        data = data.subspan(1);
                    }
                    else if (data.size() >= 2)
                    {
                        dest_port = oxenc::load_big_to_host<uint16_t>(data.data());
                        data = data.subspan(2);
                    }
                    else
                    {  // only got 1 byte total so far, need 2 for dest port
                        prev_byte = data[0];
                        return;
                    }

                    stream.pause();

                    // FIXME: TCPHandle::connect replaces the stream's data callback.  Perhaps
                    // that should happen here instead.
                    // FIXME: the connection should probably come from tun bind address, if
                    // available, rather than always 127.0.0.1
                    auto tcp_conn = TCPHandle::connect(
                        session._r.loop()->get_event_base(), FAKE_QUIC_ADDR, stream.shared_from_this(), dest_port);
                    if (!tcp_conn)
                    {
                        stream.close(11223322);  // TODO: meaningful error code
                        return;
                    }

                    _tcp_conns.push_back(tcp_conn);

                    if (data.size())
                    {
                        // put any remaining stream data on the tcp socket
                        stream.data_callback(stream, data);
                    }

                    stream.enable_watermarks(
                        500'000,
                        [this, tcp_conn](auto&) { tcp_conn->stop_reading(); },
                        50'000,
                        [this, tcp_conn](auto&) { tcp_conn->resume_reading(); });
                });
                return 0;
            };

            quic_ep = quic::Endpoint::endpoint(
                *session._r.loop(),
                FAKE_QUIC_ADDR,
                std::move(quic_send),
                std::move(new_conn),
                std::move(conn_closed),
                quic::opt::disable_mtu_discovery{});

            // TODO: only listen if we support inbound tunneled traffic
            quic_ep->listen(tls_creds, std::move(stream_opened));
        }

        void open_connection()
        {
            if (quic_conn)
            {
                log::error(
                    logcat, "Cannot create more than one QUIC connection over TPC tunnel, remote: {}", session._remote);
                return;
            }

            quic_conn = quic_ep->connect(
                quic::RemoteAddress{TUNNEL_PUBKEY, FAKE_QUIC_ADDR},
                tls_creds,
                [this](quic::Connection& conn) {
                    log::debug(logcat, "Outbound QUIC TCP Tunnel connection established to {}", session._remote);
                    if (!quic_conn)
                    {
                        quic_conn = conn.shared_from_this();
                    }
                },  // connection established
                [this, canary = std::weak_ptr{destructor_canary}](quic::Connection&, uint64_t) /* connection closed*/ {
                    // this could fire from quic::Endpoint destructor, at which point
                    // the members referenced below may no longer be valid objects
                    if (!canary.lock())
                        return;
                    if (!quic_conn)
                        log::error(logcat, "QUIC TPC tunnel connection to {} failed!", session._remote);
                    else
                        log::debug(logcat, "QUIC TPC tunnel connection to {} closed.", session._remote);
                    reset();
                });
        }

        uint16_t map_tcp_remote_port(uint16_t dest_port)
        {
            if (!session.is_active())
                return 0;
            if (!quic_conn)
            {
                open_connection();
            }

            auto _handle = TCPHandle::make_server(
                session._r.loop(), [this, dest_port](struct bufferevent* _bev, evutil_socket_t _fd) {
                    auto s =
                        quic_conn->open_stream<quic::Stream>([_bev](quic::Stream& s, std::span<const std::byte> data) {
                            auto rv = bufferevent_write(_bev, data.data(), data.size());

                            log::debug(
                                logcat,
                                "Stream (id:{}) {} {}B to TCP buffer",
                                s.stream_id(),
                                rv < 0 ? "failed to write" : "successfully wrote",
                                data.size());
                        });
                    if (!s)
                    {
                        log::error(logcat, "Failed to open stream for TCP tunnel...");
                        // FIXME: having to cast nullptr feels wrong, but idk the correct
                        // way to fix it.
                        return (TCPConnection*)nullptr;
                    }
                    std::string p;
                    p.resize(2);
                    oxenc::write_host_as_big(dest_port, p.data());
                    s->send(std::move(p));

                    auto tcp_conn = std::make_shared<TCPConnection>(_bev, _fd, std::move(s));

                    auto* ptr = tcp_conn.get();
                    _tcp_conns.push_back(std::move(tcp_conn));

                    return ptr;
                });

            auto bound_port = _handle->port();
            if (bound_port == 0)
            {
                log::error(logcat, "Failed to bind TCP port for tunneled session.");
                return 0;
            }

            log::debug(logcat, "Bound TCP tunneled session, dest_port: {}, local_port: {}", dest_port, bound_port);
            tcp_handles.emplace(dest_port, std::move(_handle));
            return bound_port;
        }
    };

    BaseSession::BaseSession(
        Router& r,
        std::shared_ptr<session_path_interface> _p,
        handlers::SessionEndpoint& parent,
        NetworkAddress remote,
        HopID remote_pivot_txid,
        session_tag _t,
        bool use_tun,
        bool is_outbound,
        shared_kx_data kx_data)
        : _r{r},
          _parent{parent},
          _tag{std::move(_t)},
          _remote{std::move(remote)},
          session_keys{std::make_unique<shared_kx_data>(std::move(kx_data))},
          _remote_pivot_txid{std::move(remote_pivot_txid)},
          _use_tun{use_tun},
          _is_outbound{is_outbound},
          _is_snode_session{_is_outbound ? !_remote.is_client() : _r.is_service_node()},
          _is_exit_session{has_flag(_tag.protocols(), protocol_flag::EXIT)}
    {
        tcp_tunnel = std::make_unique<TCPTunnel>(*this);
        set_new_current_path_interface(std::move(_p));
    }

    BaseSession::~BaseSession()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        deactivate();

        if (_current_path and _current_path->is_linked())
            _current_path->unlink_session(_tag);
    }

    bool BaseSession::send_path_control_message(
        std::string_view method, std::span<const std::byte> body, std::function<void(quic::message)> func)
    {
        auto inner_payload = PATH::CONTROL::serialize(method, body);
        auto intermediate_payload = PATH::CONTROL::serialize_aligned(as_bspan(inner_payload), _remote_pivot_txid);

        return _current_path->send_path_control_message(
            "path_control", as_bspan(intermediate_payload), std::move(func));
    }

    bool BaseSession::send_path_data_message(std::span<const std::byte> data, net::IPProtocol proto)
    {
        uint8_t type;
        if (proto == net::IPProtocol::UDP)
            type = traffic_type::UDP;
        else if (proto == net::IPProtocol::TCP)
            type = traffic_type::TCP;
        else
            type = traffic_type::RAW;

        return send_path_data_message(data, type);
    }

    bool BaseSession::send_path_data_message(std::span<const std::byte> data, uint8_t type)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        // FIXME: take a vector in here if possible to avoid extra copy
        std::vector<std::byte> data_v;
        data_v.assign(data.begin(), data.end());
        data_v.push_back(std::byte{type});
        session_keys->encrypt(data_v);

        auto intermediate_payload = PATH::DATA::serialize_intermediate(_tag, data_v, _remote_pivot_txid);

        return _current_path->send_path_data_message(as_bspan(intermediate_payload));
    }

    void BaseSession::recv_path_data_message(std::span<std::byte> data)
    {
        session_keys->decrypt(data);

        uint8_t dgram_type = std::to_integer<uint8_t>(data.back());
        data = data.subspan(0, data.size() - 1);
        bool is_udp = dgram_type == traffic_type::UDP;
        bool is_tunneled = dgram_type == traffic_type::TUNNELED_QUIC;

        if (_r.embedded())
        {
            if (is_udp)
            {
                handle_udp_from_remote(IPPacket{data});
            }
            else if (!is_tunneled)
            {
                log::warning(logcat, "Received non-UDP, non-tunneled datagram on embedded client, dropping!");
            }
            else
                tcp_tunnel->quic_ep->manually_receive_packet(oxen::quic::Packet{tcp_tunnel->FAKE_QUIC_PATH, data});
            return;
        }

        // Otherwise we're not embedded; if the other side also isn't then this is just a raw IP
        // packet to handle via the tun endpoint, and the same for UDP packets from embedded
        // remotes (which also send raw UDP packets):
        if (_use_tun || is_udp)
        {
            _r.tun_endpoint()->handle_inbound_packet(IPPacket{data}, dgram_type, _remote);
            return;
        }

        if (dgram_type == traffic_type::TUNNELED_QUIC)
        {
            tcp_tunnel->quic_ep->manually_receive_packet(oxen::quic::Packet{tcp_tunnel->FAKE_QUIC_PATH, data});
            return;
        }

        log::warning(logcat, "Received unexpected datagram with type byte: {}", dgram_type);
    }

    void BaseSession::set_new_current_path_interface(std::shared_ptr<session_path_interface> _new_path)
    {
        if (_current_path)
            _current_path->unlink_session(_tag);

        _current_path = std::move(_new_path);
        _pivot_txid = _current_path->terminal_txid();

        _current_path->link_session(_tag);
        assert(_current_path->is_linked());

        log::debug(logcat, "Session to remote ({}) set new current path {}", _remote, _current_path->to_string());
    }

    void BaseSession::set_remote_pivot_tx(HopID new_remote_txid)
    {
        log::debug(logcat, "Setting new pivot txID for remote ({}) [ new:{} ]", _remote, new_remote_txid);
        _remote_pivot_txid = std::move(new_remote_txid);
    }

    void BaseSession::publish_client_contact(const EncryptedClientContact& ecc, std::function<void(quic::message)> func)
    {
        send_path_control_message(
            "publish_cc", as_bspan(PublishClientContact::serialize(ecc, _r.local_rid())), std::move(func));
    }

    void BaseSession::handle_udp_from_remote(IPPacket&& pkt)
    {
        auto source_port = pkt.source_port();
        log::trace(logcat, "incoming udp packet from remote port {}", source_port);
        auto itr = udp_handles.find(source_port);
        if (itr == udp_handles.end())
        {
            log::debug(logcat, "Received udp datagram from unknown source port {}", source_port);
            return;
        }
        auto& socket = itr->second;
        auto dest_port = pkt.dest_port();
        log::trace(logcat, "incoming udp packet for pseudo port {}", dest_port);
        if (!udp_remote_ports.contains(dest_port))
        {
            log::warning(logcat, "Received UDP packet destined for an unmapped port ({})", dest_port);
            return;
        }
        dest_port = udp_remote_ports[dest_port];
        log::trace(logcat, "pseudo port maps to client port {}", dest_port);

        auto payload = pkt.udp_data();
        if (payload.empty())
        {
            log::warning(logcat, "Received invalid udp datagram");
            return;
        }
        quic::Address dest = socket->address();
        dest.set_port(dest_port);
        const size_t bufsize = payload.size();
        uint8_t ecn = 0;  // FIXME: do we have any way to obtain this?
        size_t n_pkts = 1;
        auto [ior, sent] = socket->send(quic::Path{socket->address(), dest}, payload.data(), &bufsize, ecn, n_pkts);
        log::trace(
            logcat, "UDP from remote -> socket send to local returned {} (ec={})", ior.success(), ior.error_code);
    }

    uint16_t BaseSession::setup_udp_mapping(uint16_t dest_port)
    {
        if (auto itr = udp_handles.find(dest_port); itr != udp_handles.end())
        {
            auto mapped_port = itr->second->address().port();
            log::debug(logcat, "Returning existing mapped port ({}) for dest port {}", mapped_port, dest_port);
            return mapped_port;
        }
        quic::Address src{"127.0.0.1"s, 0};
        quic::Address dest{"127.0.0.1"s, dest_port};
        auto udp_handle = std::make_unique<quic::UDPSocket>(
            _r.loop()->get_event_base(), src, [this, dest = std::move(dest)](quic::Packet&& pkt) {
                auto client_port = pkt.path.remote.port();
                if (!udp_client_ports.contains(client_port))
                {
                    log::debug(logcat, "Adding client port {} to mapping for remote port {}", client_port, dest.port());
                    uint16_t new_port = next_udp_client_port;
                    auto start_port = new_port;
                    while (udp_remote_ports.contains(new_port))
                    {
                        new_port++;
                        if (new_port < 1024)
                            new_port = 1024;
                        if (start_port == new_port)
                            throw std::runtime_error{"Ran out of pseudo-udp ports to use"};
                    }
                    udp_client_ports[client_port] = new_port;
                    udp_remote_ports[new_port] = client_port;
                    log::trace(logcat, "pseudo client port {} for real client port {}", new_port, client_port);
                    client_port = new_port;
                }
                else
                    client_port = udp_client_ports[client_port];

                // ip doesn't matter here, but give remote the source port so we receive responses
                // as destined for that port and know where to send them
                auto src = pkt.path.remote;
                src.set_port(client_port);
                auto payload = pkt.data();
                auto packet = IPPacket::make_udp_packet(src, dest, payload);
                send_path_data_message(packet, traffic_type::UDP);
            });
        auto bound_port = udp_handle->address().port();
        udp_handles[dest_port] = std::move(udp_handle);

        return bound_port;
    }

    uint16_t BaseSession::map_tcp_remote_port(uint16_t dest_port) { return tcp_tunnel->map_tcp_remote_port(dest_port); }

    void BaseSession::activate()
    {
        _is_active = true;
        log::debug(logcat, "Session to remote ({}) activated!", _remote);
    }

    void BaseSession::deactivate()
    {
        _is_active = false;
        log::debug(logcat, "Session to remote ({}) deactivated!", _remote);
    }

    void BaseSession::stop_session(bool send_close, std::function<void(quic::message)> func)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        deactivate();

        if (send_close)
        {
            std::promise<void> prom;

            _r.loop()->call([&]() mutable {
                send_path_close(std::move(func));
                prom.set_value();
            });

            prom.get_future().get();
            log::debug(logcat, "Dispatched path close message!");
        }

        _parent.unmap_session(_remote);
    }

    static void session_close_cb(quic::message m)
    {
        log::debug(logcat, "Remote {} session", m ? "successfully closed" : "failed to close");
    }

    void BaseSession::send_path_close(std::function<void(quic::message)> func)
    {
        log::debug(logcat, "Dispatching close session message...");
        send_path_control_message(
            "session_close", as_bspan(CloseSession::serialize(_tag)), func ? std::move(func) : session_close_cb);
    }

    std::string BaseSession::to_string() const
    {
        return "{}BSession:[ active:{} | exit:{} | {} ]"_format(
            _is_outbound ? "O" : "I", _is_active, _is_exit_session, _current_path->to_string());
    }

    OutboundRelaySession::OutboundRelaySession(
        NetworkAddress remote,
        handlers::SessionEndpoint& parent,
        std::shared_ptr<path::Path> path,
        session_tag _t,
        HopID remote_pivot_txid,
        shared_kx_data kx_data)
        : PathHandler{parent._router, path::DEFAULT_PATHS_HELD},
          BaseSession(
              _router,
              std::move(path),
              parent,
              std::move(remote),
              std::move(remote_pivot_txid),
              std::move(_t),
              !_router.embedded(),
              true,
              std::move(kx_data)),
          _last_use{_router.now()}
    {
        add_path(current_path());

        _path_rotater = _router.loop()->call_every(path::PATH_ROTATION_INTERVAL, [this] { rotate_paths(); });
    }

    std::shared_ptr<path::Path> OutboundRelaySession::current_path()
    {
        return std::dynamic_pointer_cast<path::Path>(_current_path);
    }

    std::shared_ptr<path::PathHandler> OutboundRelaySession::get_self() { return shared_from_this(); }

    std::weak_ptr<path::PathHandler> OutboundRelaySession::get_weak() { return weak_from_this(); }

    bool OutboundRelaySession::send_path_control_message(
        std::string_view method, std::span<const std::byte> body, std::function<void(quic::message)> func)
    {
        return _current_path->send_path_control_message(method, body, std::move(func));
    }

    void OutboundRelaySession::rotate_paths()
    {
        log::debug(logcat, "{} called", __PRETTY_FUNCTION__);

        Lock_t l{paths_mutex};

        auto rid = _current_path->terminal_rid();

        // TODO FIXME: looping here makes no sense; if selecting aligned hops fail calling it again
        // right away isn't going to change anything.
        for (int i = 0; i < SESSION_PATH_BUILD_ATTEMPTS; ++i)
        {
            auto maybe_hops = aligned_hops_to_remote(rid);

            if (maybe_hops)
                return path::PathHandler::rotate_paths(std::move(*maybe_hops));

            log::warning(logcat, "Failed attempt #{} to get hops for path-build to remote: {}", i + 1, rid);
        }
    }

    void OutboundRelaySession::select_new_current()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        switch_to_new_path(get_newest_path());
    }

    void OutboundRelaySession::switch_to_new_path(std::shared_ptr<path::Path> p)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        set_remote_pivot_tx(p->pivot().txid());
        set_new_current_path_interface(std::move(p));
        send_path_switch();
    }

    void OutboundRelaySession::drop_oldest_path()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        Lock_t l{paths_mutex};

        auto oldest = get_oldest_path();
        log::debug(logcat, "Dropping oldest path: {}", oldest->to_string());

        bool set_new_current = oldest == _current_path;

        drop_path(oldest);

        if (set_new_current)
            select_new_current();
        else
            log::debug(logcat, "Dropped oldest path; current path is still valid...");
    }

    void OutboundRelaySession::build_more(size_t n)
    {
        size_t count{0};
        log::critical(
            logcat, "OutboundRelaySession building {} paths to remote to have a minimum of {}", n, num_paths_desired);

        while (count < n)
            count += build_path_aligned_to_remote(_current_path->terminal_rid());

        if (count == n)
            log::debug(logcat, "SessionEndpoint successfully initiated {} path-builds", n);
        else
            log::warning(logcat, "SessionEndpoint only initiated {} path-builds (needed: {})", count, n);
    }

    void OutboundRelaySession::send_path_switch()
    {
        log::debug(logcat, "Dispatching path-switch request to remote ({}): {}", _remote, _current_path->to_string());
        send_path_control_message(
            "path_switch",
            as_bspan(SessionPathSwitch::serialize(_tag, _current_path->terminal_txid(), _remote_pivot_txid)),
            [](quic::message m) {
                if (m)
                    log::info(logcat, "Session path switch was successful!");
                else
                {
                    std::optional<std::string> status = std::nullopt;
                    try
                    {
                        oxenc::bt_dict_consumer btdc{m.body()};

                        if (auto s = btdc.maybe<std::string>(messages::STATUS_KEY))
                            status = s;
                    }
                    catch (const std::exception& e)
                    {
                        log::warning(logcat, "Exception: {}", e.what());
                    }

                    log::critical(logcat, "Session path switch FAILED; reason: {}", status.value_or("<none given>"));
                }
            });
    }

    void OutboundRelaySession::stop(bool send_close)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        stop_session(send_close);
    }

    void OutboundRelaySession::stop_session(bool send_close, std::function<void(quic::message)> func)
    {
        log::debug(logcat, "{} called", __PRETTY_FUNCTION__);

        _running = false;

        if (_path_rotater)
        {
            if (_path_rotater->is_running())
                _path_rotater->stop();

            _path_rotater.reset();
            log::trace(logcat, "Path rotation ticker stopped!");
        }

        std::vector<HopID> droplist{};
        {
            Lock_t l{paths_mutex};
            std::ranges::for_each(_paths, [&droplist](auto p) {
                droplist.emplace_back(p.second->edge().rxid());
                droplist.emplace_back(p.second->pivot().txid());
            });
            log::debug(logcat, "Session droplist holds {} paths", droplist.size());
        }

        _router.loop()->call_soon([wparent = _parent.get_weak(), droplist = std::move(droplist)]() mutable {
            if (auto parent = wparent.lock())
                parent->router().path_context.drop_paths(std::move(droplist));
            else
                log::warning(logcat, "SessionEndpoint died before dropping session paths");
        });

        BaseSession::stop_session(send_close, std::move(func));
        path::PathHandler::stop();
    }

    OutboundClientSession::OutboundClientSession(
        NetworkAddress remote,
        handlers::SessionEndpoint& parent,
        std::shared_ptr<path::Path> path,
        HopID remote_pivot_txid,
        session_tag _t,
        sorted_intro_set _remote_intros,
        shared_kx_data kx_data)
        : OutboundRelaySession{
              std::move(remote),
              parent,
              std::move(path),
              std::move(_t),
              std::move(remote_pivot_txid),
              std::move(kx_data)}
    {
        // These can both be false but CANNOT both be true
        if (_is_exit_session and _is_snode_session)
            throw std::runtime_error{"Cannot create OutboundSession for a remote exit and remote service node!"};

        // _path_rotater = _router.loop()->call_every(path::PATH_ROTATION_INTERVAL, [this]() mutable { rotate_paths();
        // }); add_path(current_path());

        populate_intro_map(std::move(_remote_intros));

        log::debug(
            logcat,
            "OutboundSession to remote {} {} created...",
            _is_snode_session ? "relay" : "client",
            _is_exit_session ? "exit" : "service");
    }

    std::shared_ptr<path::PathHandler> OutboundClientSession::get_self() { return shared_from_this(); }

    std::weak_ptr<path::PathHandler> OutboundClientSession::get_weak() { return weak_from_this(); }

    bool OutboundClientSession::send_path_control_message(
        std::string_view method, std::span<const std::byte> body, std::function<void(quic::message)> func)
    {
        return BaseSession::send_path_control_message(method, body, std::move(func));
    }

    void OutboundClientSession::populate_intro_map(const sorted_intro_set& _remote_intros)
    {
        log::trace(logcat, "Populating intro map for {} intros!", _remote_intros.size());
        Lock_t l(paths_mutex);

        intro_path_mapping.clear();

        for (auto& intro : _remote_intros)
        {
            log::critical(logcat, "intro: {}", intro);
            if (intro.pivot_txid == _remote_pivot_txid)
                intro_path_mapping.emplace(intro, path::PathPtrSet{current_path()});
            else
                intro_path_mapping.emplace(intro, path::PathPtrSet{});
        }
    }

    void OutboundClientSession::update_outbound_remote_intros(sorted_intro_set intros)
    {
        log::debug(logcat, "Updating ClientIntros for OutboundSession to remote: {}", _remote);
        /**
            - Clear intro_path_map
            - Check path_handler map for any paths to the new intro_set pivots
                - If so:
                    - add to new intro_path_map
                    - make new current path
                - Else:
                    - build and switch
         */

        populate_intro_map(intros);

        if (not update_local_paths())
        {
            log::debug(logcat, "No valid paths left for new intros; building new paths...");
            build_and_switch_paths(std::move(intros));
        }
    }

    bool OutboundClientSession::update_local_paths()
    {
        Lock_t l(paths_mutex);

        bool find_new_current = false;

        for (auto it = _paths.begin(); it != _paths.end();)
        {
            bool keep_path = false;
            const auto& remote_pivot = it->second->pivot().router_id();

            for (auto& [intro, pathset] : intro_path_mapping)
            {
                if (intro.pivot_rid == remote_pivot)
                {
                    pathset.emplace(it->second);
                    keep_path = true;
                    break;
                }
            }

            if (keep_path)
                ++it;
            else
            {
                // log::debug(logcat, "Unlinking and dropping path {}", *it->second);
                log::debug(logcat, "Unlinking and dropping path {}", it->second->to_string());
                // it->second->unlink_session(_tag);
                _router.path_context.drop_path(*it->second);

                if (it->second == _current_path)
                {
                    log::debug(logcat, "Dropped path is current session path");
                    find_new_current = true;
                }

                it = _paths.erase(it);
            }
        }

        //  If _paths is empty, we need to build a new current to switch to. Otherwise, if we
        //  had to drop our current path, we need to try to select a new one from the non-empty
        //  _paths map.
        return _paths.empty() ? false : find_new_current ? select_new_current() : true;
    }

    void OutboundClientSession::switch_to_new_path(std::shared_ptr<path::Path> p, HopID new_pivot_txid)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        set_remote_pivot_tx(std::move(new_pivot_txid));
        set_new_current_path_interface(std::move(p));
        send_path_switch();
    }

    bool OutboundClientSession::select_new_current()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        for (auto& [intro, pathset] : intro_path_mapping)
        {
            if (pathset.empty())
                continue;

            for (auto& p : pathset)
            {
                if (p != _current_path)
                {
                    switch_to_new_path(p, intro.pivot_txid);
                    return true;
                }
            }
        }

        log::debug(logcat, "Failed to find valid new current path in intro path mapping...");
        return false;
    }

    nlohmann::json OutboundClientSession::ExtractStatus() const
    {
        auto obj = path::PathHandler::ExtractStatus();
        obj["lastExitUse"] = to_json(_last_use);
        // auto pub = _auth->session_key().to_pubkey();
        // obj["exitIdentity"] = pub.to_string();
        obj["endpoint"] = _remote.to_string();
        return obj;
    }

    void OutboundClientSession::map_path(const std::shared_ptr<path::Path>& p)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        for (auto& [intro, pathset] : intro_path_mapping)
        {
            if (intro.pivot_rid == p->pivot().router_id())
            {
                pathset.emplace(p);
                log::debug(logcat, "Client intro {} has {} paths to remote pivot", intro, pathset.size());
                return;
            }
        }

        log::warning(logcat, "Could not match currently held intros to path over pivot ({})", p->pivot().router_id());
    }

    bool OutboundClientSession::unmap_path(const std::shared_ptr<path::Path>& p)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        for (auto& [intro, pathset] : intro_path_mapping)
        {
            if (intro.pivot_rid == p->pivot().router_id())
            {
                // p->unlink_session(_tag);
                pathset.erase(p);
                log::debug(logcat, "Client intro {} has {} paths to remote pivot", intro, pathset.size());
                return true;
            }
        }

        log::warning(logcat, "Could not match currently held intros to path over pivot ({})", p->pivot().router_id());
        return false;
    }

    void OutboundClientSession::path_build_succeeded(const std::shared_ptr<path::Path>& p)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        Lock_t l(paths_mutex);

        map_path(p);
        path::PathHandler::path_build_succeeded(p);
    }

    void OutboundClientSession::path_build_failed(const std::shared_ptr<path::Path>& p, bool timeout)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        Lock_t l(paths_mutex);

        unmap_path(p);
        path::PathHandler::path_build_failed(p, timeout);
    }

    void OutboundClientSession::stop_session(bool send_close, std::function<void(quic::message)> func)
    {
        log::debug(logcat, "{} called", __PRETTY_FUNCTION__);

        intro_path_mapping.clear();

        OutboundRelaySession::stop_session(send_close, std::move(func));
    }

    void OutboundClientSession::build_more(size_t n)
    {
        size_t count{0};

        log::critical(
            logcat,
            "OutboundSession building {} paths to have a minimum of {} ({} intros held)",
            n,
            path::DEFAULT_PATHS_HELD,
            intro_path_mapping.size());

        for (auto& [intro, pathset] : intro_path_mapping)
        {
            if (pathset.size() >= PATHS_PER_INTRO)
            {
                log::trace(
                    logcat, "OutboundSession already holds {} paths for intro to pivot {}", pathset.size(), intro);
                continue;
            }

            auto needed = PATHS_PER_INTRO - pathset.size();
            for (size_t i = 0; i < needed && count < n; ++i)
                count += build_path_aligned_to_remote(intro.pivot_rid);

            log::trace(
                logcat,
                "OutboundSession built {} path(s) to have {} for intro to pivot {}",
                needed,
                PATHS_PER_INTRO,
                intro);

            if (count >= n)
                break;
        }

        if (count >= n)
            log::debug(logcat, "OutboundSession successfully initiated {} path-builds", count);
        else
            log::warning(logcat, "OutboundSession only initiated {} path-builds (needed: {})", count, n);
    }

    void OutboundClientSession::build_and_switch_paths()
    {
        log::debug(logcat, "{} called", __PRETTY_FUNCTION__);

        auto intros = get_local_client_intros();
        assert(not intros.empty());

        return build_and_switch_paths(std::move(intros));
    }

    void OutboundClientSession::build_and_switch_paths(sorted_intro_set intros)
    {
        log::debug(logcat, "{} called", __PRETTY_FUNCTION__);

        path_build_recursive(
            intros,
            _remote,
            [this](std::shared_ptr<path::Path> new_path, ClientIntro intro) mutable {
                path_build_succeeded(new_path);
                return switch_to_new_path(std::move(new_path), std::move(intro.pivot_txid));
            },
            true);
    }

    void OutboundClientSession::drop_oldest_path()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        Lock_t l{paths_mutex};

        auto oldest = get_oldest_path();
        log::debug(logcat, "Dropping oldest path: {}", oldest->to_string());

        bool found = unmap_path(oldest), set_new_current = oldest == _current_path;

        log::debug(logcat, "{}uccessfully dropped oldest path from OutboundSession mapping", found ? "S" : "Uns");

        drop_path(oldest);

        if (not set_new_current)
        {
            log::debug(logcat, "Dropped oldest path; current path is still valid...");
            return;
        }

        if (select_new_current())
            return;

        build_and_switch_paths();
    }

    void OutboundClientSession::rotate_paths()
    {
        log::debug(logcat, "{} called", __PRETTY_FUNCTION__);

        Lock_t l{paths_mutex};

        // use the newest intro we have for the remote, and build a path to that pivot
        const auto& rid = intro_path_mapping.begin()->first.pivot_rid;

        // TODO FIXME: looping here makes no sense; if selecting aligned hops fail calling it again
        // right away isn't going to change anything.
        for (int i = 0; i < SESSION_PATH_BUILD_ATTEMPTS; ++i)
        {
            auto maybe_hops = aligned_hops_to_remote(rid);

            if (maybe_hops)
                return path::PathHandler::rotate_paths(std::move(*maybe_hops));

            log::warning(logcat, "Failed attempt #{} to get hops for path-build to remote: {}", i + 1, rid);
        }
    }

    bool OutboundClientSession::is_ready() const
    {
        if (_pivot_txid.is_zero())
            return false;

        const size_t expect = (1 + (num_paths_desired / 2));

        return num_active_paths() >= expect;
    }

    bool OutboundClientSession::is_expired(std::chrono::milliseconds now) const
    {
        return now > _last_use && now - _last_use > path::DEFAULT_LIFETIME;
    }

    InboundClientSession::InboundClientSession(
        NetworkAddress remote,
        std::shared_ptr<session_path_interface> _p,
        handlers::SessionEndpoint& parent,
        HopID remote_pivot_txid,
        session_tag _t,
        bool use_tun,
        shared_kx_data kx_data)
        : BaseSession{
              parent._router,
              std::move(_p),
              parent,
              std::move(remote),
              std::move(remote_pivot_txid),
              std::move(_t),
              use_tun,
              false,
              std::move(kx_data)}
    {
        log::debug(
            logcat,
            "InboundSession from remote client to local client {} created",
            _is_exit_session ? "exit" : "service");
    }

    void InboundClientSession::recv_path_switch(HopID remote_pivot_txid, std::shared_ptr<session_path_interface> new_pi)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        set_remote_pivot_tx(std::move(remote_pivot_txid));
        set_new_current_path_interface(std::move(new_pi));
    }

    InboundRelaySession::InboundRelaySession(
        NetworkAddress remote,
        std::shared_ptr<session_path_interface> _p,
        handlers::SessionEndpoint& parent,
        HopID remote_pivot_txid,
        session_tag _t,
        bool use_tun,
        shared_kx_data kx_data)
        : InboundClientSession{
              std::move(remote),
              std::move(_p),
              parent,
              std::move(remote_pivot_txid),
              std::move(_t),
              use_tun,
              std::move(kx_data)}
    {
        log::debug(logcat, "InboundSession from remote client to local relay service created");
    }

    bool InboundRelaySession::send_path_control_message(
        std::string_view method, std::span<const std::byte> body, std::function<void(quic::message)> func)
    {
        return _current_path->send_path_control_message(method, body, std::move(func));
    }

    bool InboundRelaySession::send_path_data_message(std::span<const std::byte> data, uint8_t type)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        auto tag = _tag.span();

        std::vector<std::byte> data_v;
        data_v.resize(data.size() + 1 /* type */ + tag.size());
        std::memcpy(data_v.data() + tag.size(), data.data(), data.size());
        data_v.back() = std::byte{type};
        session_keys->encrypt(std::span<std::byte>{data_v.data() + tag.size(), data_v.size() - tag.size()});

        std::memcpy(data_v.data(), tag.data(), tag.size());

        return _current_path->send_path_data_message(data_v);
    }

}  // namespace llarp::session
