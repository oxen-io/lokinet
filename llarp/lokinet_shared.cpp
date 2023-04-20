#include <lokinet.h>
#include <algorithm>
#include <exception>
#include <filesystem>
#include <llarp.hpp>
#include <llarp/config/config.hpp>
#include <llarp/crypto/crypto_libsodium.hpp>

#include <llarp/router/abstractrouter.hpp>
#include <llarp/service/context.hpp>
#include <llarp/quic/tunnel.hpp>
#include <llarp/nodedb.hpp>

#include <llarp/util/logging.hpp>
#include <llarp/util/logging/buffer.hpp>
#include <llarp/util/logging/callback_sink.hpp>

#include <oxenc/base32z.h>

#include <mutex>
#include <memory>
#include <chrono>
#include <stdexcept>
#include <unordered_map>
#include "lokinet/lokinet_tcp.h"

#ifdef _WIN32
#define EHOSTDOWN ENETDOWN
#endif

namespace
{
  static auto logcat = llarp::log::Cat("liblokinet");

  struct Context : public llarp::Context
  {
    using llarp::Context::Context;

    std::shared_ptr<llarp::NodeDB>
    makeNodeDB() override
    {
      return llarp::Context::makeNodeDB();
    }
  };

  struct UDPFlow
  {
    using Clock_t = std::chrono::steady_clock;
    void* m_FlowUserData;
    std::chrono::seconds m_FlowTimeout;
    std::chrono::time_point<Clock_t> m_ExpiresAt;
    lokinet_udp_flowinfo m_FlowInfo;
    lokinet_udp_flow_recv_func m_Recv;

    /// call timeout hook for this flow
    void
    TimedOut(lokinet_udp_flow_timeout_func timeout)
    {
      timeout(&m_FlowInfo, m_FlowUserData);
    }

    /// mark this flow as active
    /// updates the expires at timestamp
    void
    MarkActive()
    {
      m_ExpiresAt = Clock_t::now() + m_FlowTimeout;
    }

    /// returns true if we think this flow is expired
    bool
    IsExpired() const
    {
      return Clock_t::now() >= m_ExpiresAt;
    }

    void
    HandlePacket(const llarp::net::IPPacket& pkt)
    {
      if (auto maybe = pkt.L4Data())
      {
        MarkActive();
        m_Recv(&m_FlowInfo, maybe->first, maybe->second, m_FlowUserData);
      }
    }
  };

  struct UDPHandler
  {
    using AddressVariant_t = llarp::vpn::AddressVariant_t;
    int m_SocketID;
    llarp::nuint16_t m_LocalPort;
    lokinet_udp_flow_filter m_Filter;
    lokinet_udp_flow_recv_func m_Recv;
    lokinet_udp_flow_timeout_func m_Timeout;
    void* m_User;
    std::weak_ptr<llarp::service::Endpoint> m_Endpoint;

    std::unordered_map<AddressVariant_t, UDPFlow> m_Flows;

    std::mutex m_Access;

    explicit UDPHandler(
        int socketid,
        llarp::nuint16_t localport,
        lokinet_udp_flow_filter filter,
        lokinet_udp_flow_recv_func recv,
        lokinet_udp_flow_timeout_func timeout,
        void* user,
        std::weak_ptr<llarp::service::Endpoint> ep)
        : m_SocketID{socketid}
        , m_LocalPort{localport}
        , m_Filter{filter}
        , m_Recv{recv}
        , m_Timeout{timeout}
        , m_User{user}
        , m_Endpoint{std::move(ep)}
    {}

    void
    KillAllFlows()
    {
      std::unique_lock lock{m_Access};
      for (auto& item : m_Flows)
      {
        item.second.TimedOut(m_Timeout);
      }
      m_Flows.clear();
    }

    void
    AddFlow(
        const AddressVariant_t& from,
        const lokinet_udp_flowinfo& flow_addr,
        void* flow_userdata,
        int flow_timeoutseconds,
        std::optional<llarp::net::IPPacket> firstPacket = std::nullopt)
    {
      std::unique_lock lock{m_Access};
      auto& flow = m_Flows[from];
      flow.m_FlowInfo = flow_addr;
      flow.m_FlowTimeout = std::chrono::seconds{flow_timeoutseconds};
      flow.m_FlowUserData = flow_userdata;
      flow.m_Recv = m_Recv;
      if (firstPacket)
        flow.HandlePacket(*firstPacket);
    }

    void
    ExpireOldFlows()
    {
      std::unique_lock lock{m_Access};
      for (auto itr = m_Flows.begin(); itr != m_Flows.end();)
      {
        if (itr->second.IsExpired())
        {
          itr->second.TimedOut(m_Timeout);
          itr = m_Flows.erase(itr);
        }
        else
          ++itr;
      }
    }

    void
    HandlePacketFrom(AddressVariant_t from, llarp::net::IPPacket pkt)
    {
      {
        std::unique_lock lock{m_Access};
        if (m_Flows.count(from))
        {
          m_Flows[from].HandlePacket(pkt);
          return;
        }
      }
      lokinet_udp_flowinfo flow_addr{};
      // set flow remote address
      std::string addrstr = var::visit([](auto&& from) { return from.ToString(); }, from);

      std::copy_n(
          addrstr.data(),
          std::min(addrstr.size(), sizeof(flow_addr.remote_host)),
          flow_addr.remote_host);
      // set socket id
      flow_addr.socket_id = m_SocketID;
      // get source port
      if (const auto srcport = pkt.SrcPort())
      {
        flow_addr.remote_port = ToHost(*srcport).h;
      }
      else
        return;  // invalid data so we bail
      void* flow_userdata = nullptr;
      int flow_timeoutseconds{};
      // got a new flow, let's check if we want it
      if (m_Filter(m_User, &flow_addr, &flow_userdata, &flow_timeoutseconds))
        return;
      AddFlow(from, flow_addr, flow_userdata, flow_timeoutseconds, pkt);
    }
  };
}  // namespace

struct lokinet_context
{
  std::mutex m_access;

  std::shared_ptr<llarp::Context> impl = std::make_shared<Context>();
  std::shared_ptr<llarp::Config> config = llarp::Config::EmbeddedConfig();

  std::unique_ptr<std::thread> runner;

  int _socket_id = 0;

  ~lokinet_context()
  {
    if (runner)
      runner->join();
  }

  int
  next_socket_id()
  {
    int id = ++_socket_id;
    // handle overflow
    if (id < 0)
    {
      _socket_id = 0;
      id = ++_socket_id;
    }
    return id;
  }

  /// make a udp handler and hold onto it
  /// return its id
  [[nodiscard]] std::optional<int>
  make_udp_handler(
      const std::shared_ptr<llarp::service::Endpoint>& ep,
      llarp::net::port_t exposePort,
      lokinet_udp_flow_filter filter,
      lokinet_udp_flow_recv_func recv,
      lokinet_udp_flow_timeout_func timeout,
      void* user)
  {
    if (udp_sockets.empty())
    {
      // start udp flow expiration timer
      impl->router->loop()->call_every(1s, std::make_shared<int>(0), [this]() {
        std::unique_lock lock{m_access};
        for (auto& item : udp_sockets)
        {
          item.second->ExpireOldFlows();
        }
      });
    }
    std::weak_ptr<llarp::service::Endpoint> weak{ep};
    auto udp = std::make_shared<UDPHandler>(
        next_socket_id(), exposePort, filter, recv, timeout, user, weak);
    auto id = udp->m_SocketID;
    std::promise<bool> result;

    impl->router->loop()->call([ep, &result, udp, exposePort]() {
      if (auto pkt = ep->EgresPacketRouter())
      {
        pkt->AddUDPHandler(llarp::net::ToHost(exposePort), [udp](auto from, auto pkt) {
          udp->HandlePacketFrom(std::move(from), std::move(pkt));
        });
        result.set_value(true);
      }
      else
        result.set_value(false);
    });

    if (result.get_future().get())
    {
      udp_sockets[udp->m_SocketID] = std::move(udp);
      return id;
    }
    return std::nullopt;
  }

  void
  remove_udp_handler(int socket_id)
  {
    std::shared_ptr<UDPHandler> udp;
    {
      std::unique_lock lock{m_access};
      if (auto itr = udp_sockets.find(socket_id); itr != udp_sockets.end())
      {
        udp = std::move(itr->second);
        udp_sockets.erase(itr);
      }
    }
    if (udp)
    {
      udp->KillAllFlows();
      // remove packet handler
      impl->router->loop()->call(
          [ep = udp->m_Endpoint.lock(), localport = llarp::ToHost(udp->m_LocalPort)]() {
            if (auto pkt = ep->EgresPacketRouter())
              pkt->RemoveUDPHandler(localport);
          });
    }
  }

  /// acquire mutex for accessing this context
  [[nodiscard]] auto
  acquire()
  {
    return std::unique_lock{m_access};
  }

  [[nodiscard]] auto
  endpoint(std::string name = "default") const
  {
    return impl->router->hiddenServiceContext().GetEndpointByName(name);
  }

  /// false: outbound connection
  /// true: inbound connection
  std::unordered_map<int, bool> tcp_conns;
  /// maps address to pair of (stream_id, ready)
  std::unordered_map<std::string, lokinet_tcp_result> active_conns;

  std::unordered_map<int, std::shared_ptr<UDPHandler>> udp_sockets;

  void
  inbound_tcp(int id)
  {
    tcp_conns[id] = true;
  }

  void
  outbound_tcp(std::string remote_addr, lokinet_tcp_result& res)
  {
    tcp_conns[res.tcp_id] = false;
    active_conns[remote_addr] = lokinet_tcp_result{res};
  }
};

namespace
{
  void
  tcp_error(lokinet_tcp_result* result, int err)
  {
    *result = lokinet_tcp_result{};
    result->error = err;
  }

  void
  tcp_okay(lokinet_tcp_result* result, std::string host, int port, int tcp_id)
  {
    tcp_error(result, 0);
    std::copy_n(
        host.c_str(),
        std::min(host.size(), sizeof(result->local_address) - 1),
        result->local_address);
    result->local_port = port;
    result->tcp_id = tcp_id;
  }

  std::pair<std::string, int>
  split_host_port(std::string data, std::string proto = "tcp")
  {
    std::string host, portStr;
    if (auto pos = data.find(":"); pos != std::string::npos)
    {
      host = data.substr(0, pos);
      portStr = data.substr(pos + 1);
    }
    else
      throw std::invalid_argument("Error: invalid address passed");

    if (auto* serv = getservbyname(portStr.c_str(), proto.c_str()))
    {
      return {host, serv->s_port};
    }
    return {host, std::stoi(portStr)};
  }

  int
  accept_port(const char* remote, uint16_t port, void* ptr)
  {
    (void)remote;
    if (port == *static_cast<uint16_t*>(ptr))
    {
      return 0;
    }
    return -1;
  }

  std::optional<lokinet_srv_record>
  SRVFromData(const llarp::dns::SRVData& data, std::string name)
  {
    // TODO: implement me
    (void)data;
    (void)name;
    return std::nullopt;
  }

}  // namespace

struct lokinet_srv_lookup_private
{
  std::vector<lokinet_srv_record> results;

  int
  LookupSRV(std::string host, std::string service, lokinet_context* ctx)
  {
    std::promise<int> promise;
    {
      auto lock = ctx->acquire();
      if (ctx->impl and ctx->impl->IsUp())
      {
        ctx->impl->CallSafe([host, service, &promise, ctx, this]() {
          auto ep = ctx->endpoint();
          if (ep == nullptr)
          {
            promise.set_value(ENOTSUP);
            return;
          }
          ep->LookupServiceAsync(host, service, [this, &promise, host](auto results) {
            for (const auto& result : results)
            {
              if (auto maybe = SRVFromData(result, host))
                this->results.emplace_back(*maybe);
            }
            promise.set_value(0);
          });
        });
      }
      else
      {
        promise.set_value(EHOSTDOWN);
      }
    }
    auto future = promise.get_future();
    return future.get();
  }

  void
  IterateAll(std::function<void(lokinet_srv_record*)> visit)
  {
    for (auto& result : results)
      visit(&result);
    // null terminator
    visit(nullptr);
  }
};

extern "C"
{
  void EXPORT
  lokinet_set_netid(const char* netid)
  {
    llarp::NetID::DefaultValue() = llarp::NetID{reinterpret_cast<const byte_t*>(netid)};
  }

  const char* EXPORT
  lokinet_get_netid()
  {
    const auto netid = llarp::NetID::DefaultValue().ToString();
    return strdup(netid.c_str());
  }

  static auto last_log_set = llarp::log::Level::info;

  int EXPORT
  lokinet_log_level(const char* level)
  {
    try
    {
      auto new_level = llarp::log::level_from_string(level);
      llarp::log::reset_level(new_level);
      last_log_set = new_level;
      return 0;
    }
    catch (std::invalid_argument& e)
    {
      llarp::LogError(e.what());
    }
    return -1;
  }

  char* EXPORT
  lokinet_address(struct lokinet_context* ctx)
  {
    if (not ctx)
      return nullptr;
    auto lock = ctx->acquire();
    auto ep = ctx->endpoint();
    const auto addr = ep->GetIdentity().pub.Addr();
    const auto addrStr = addr.ToString();
    return strdup(addrStr.c_str());
  }

  int EXPORT
  lokinet_add_bootstrap_rc(const char* data, size_t datalen, struct lokinet_context* ctx)
  {
    // FIXME: bootstrap loading was rewritten but this code needs updated to do
    //        it how Router does now.
    if (data == nullptr or datalen == 0)
      return -3;
    llarp_buffer_t buf{data, datalen};
    if (ctx == nullptr)
      return -3;
    auto lock = ctx->acquire();
    // add a temp cryptography implementation here so rc.Verify works
    llarp::CryptoManager instance{new llarp::sodium::CryptoLibSodium{}};
    if (data[0] == 'l')
    {
      if (not ctx->config->bootstrap.routers.BDecode(&buf))
      {
        llarp::LogError("Cannot decode bootstrap list: ", llarp::buffer_printer{buf});
        return -1;
      }
      for (const auto& rc : ctx->config->bootstrap.routers)
      {
        if (not rc.Verify(llarp::time_now_ms()))
          return -2;
      }
    }
    else
    {
      llarp::RouterContact rc{};
      if (not rc.BDecode(&buf))
      {
        llarp::LogError("failed to decode signle RC: ", llarp::buffer_printer{buf});
        return -1;
      }
      if (not rc.Verify(llarp::time_now_ms()))
        return -2;
      ctx->config->bootstrap.routers.insert(std::move(rc));
    }
    return 0;
  }

  struct lokinet_context* EXPORT
  lokinet_context_new()
  {
    return new lokinet_context{};
  }

  void EXPORT
  lokinet_context_free(struct lokinet_context* ctx)
  {
    lokinet_context_stop(ctx);
    delete ctx;
  }

  int EXPORT
  lokinet_context_start(struct lokinet_context* ctx)
  {
    if (not ctx)
      return -1;
    auto lock = ctx->acquire();
    ctx->config->router.m_netId = lokinet_get_netid();
    ctx->config->logging.m_logLevel = last_log_set;
    ctx->runner = std::make_unique<std::thread>([ctx]() {
      llarp::util::SetThreadName("llarp-mainloop");
      ctx->impl->Configure(ctx->config);
      const llarp::RuntimeOptions opts{};
      try
      {
        ctx->impl->Setup(opts);
#ifdef SIG_PIPE
        signal(SIG_PIPE, SIGIGN);
#endif
        ctx->impl->Run(opts);
      }
      catch (std::exception& ex)
      {
        std::cerr << ex.what() << std::endl;
        ctx->impl->CloseAsync();
      }
    });
    while (not ctx->impl->IsUp())
    {
      if (ctx->impl->IsStopping())
        return -1;
      std::this_thread::sleep_for(50ms);
    }
    return 0;
  }

  int EXPORT
  lokinet_status(struct lokinet_context* ctx)
  {
    if (ctx == nullptr)
      return -3;
    auto lock = ctx->acquire();
    if (not ctx->impl->IsUp())
      return -3;
    if (not ctx->impl->LooksAlive())
      return -2;
    return ctx->endpoint()->IsReady() ? 0 : -1;
  }

  int EXPORT
  lokinet_wait_for_ready(int ms, struct lokinet_context* ctx)
  {
    if (ctx == nullptr)
      return -1;
    auto lock = ctx->acquire();
    auto ep = ctx->endpoint();
    int iterations = ms / 10;
    if (iterations <= 0)
    {
      ms = 10;
      iterations = 1;
    }
    while (not ep->IsReady() and iterations > 0)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds{ms / 10});
      iterations--;
    }
    return ep->IsReady() ? 0 : -1;
  }

  void EXPORT
  lokinet_context_stop(struct lokinet_context* ctx)
  {
    if (not ctx)
      return;
    auto lock = ctx->acquire();

    if (ctx->impl->IsStopping())
      return;

    ctx->impl->CloseAsync();
    ctx->impl->Wait();

    if (ctx->runner)
      ctx->runner->join();

    ctx->runner.reset();
  }

  void EXPORT
  lokinet_set_data_dir(const char* path, struct lokinet_context* ctx)
  {
    fs::path dir{path};
    dir = fs::canonical(dir);
    fs::current_path(dir);

    if (not ctx)
      return;
    auto lock = ctx->acquire();

    if (ctx->impl->IsUp() or ctx->impl->IsStopping())
      return;

    ctx->config->router.m_dataDir = dir;
  }

  void EXPORT
  lokinet_outbound_tcp(
      struct lokinet_tcp_result* result,
      const char* remote,
      const char* local,
      struct lokinet_context* ctx,
      void (*open_cb)(bool success, void* user_data),
      void (*close_cb)(int rv, void* user_data),
      void* user_data)
  {
    if (ctx == nullptr)
    {
      tcp_error(result, EHOSTDOWN);
      return;
    }

    if (auto itr = ctx->active_conns.find(remote); itr != ctx->active_conns.end())
    {
      result->success = true;
      llarp::LogError("Active connection to {} already exists", remote);
      return;
    }

    auto lock = ctx->acquire();

    if (not ctx->impl->IsUp())
    {
      tcp_error(result, EHOSTDOWN);
      return;
    }
    std::string remotehost;
    int remoteport;
    try
    {
      auto [h, p] = split_host_port(remote);
      remotehost = h;
      remoteport = p;
    }
    catch (std::exception& e)
    {
      llarp::log::error(logcat, "Error: exception caught: {}", e.what());
      tcp_error(result, EINVAL);
      return;
    }
    // TODO: make configurable (?)
    // FIXME: appears unused?
    std::string endpoint{"default"};

    llarp::SockAddr localAddr;
    try
    {
      if (local)
        localAddr = llarp::SockAddr{std::string{local}};
      else
        localAddr = llarp::SockAddr{"127.0.0.1:0"};
    }
    catch (std::exception& ex)
    {
      tcp_error(result, EINVAL);
      return;
    }

    auto on_open = [result, localAddr, remotehost, remoteport, open_cb](
                       bool success, void* user_data) {
      llarp::log::info(
          logcat,
          "Quic tunnel {}<->{}:{} {}.",
          localAddr,
          remotehost,
          remoteport,
          success ? "opened successfully" : "failed");

      result->success = success;

      if (open_cb)
        open_cb(success, user_data);
    };

    auto on_close = [&ctx, localAddr, remote, close_cb](int rv, void* user_data) {
      llarp::log::info(logcat, "Quic tunnel {}<->{} closed.", localAddr, remote);

      ctx->active_conns.erase(remote);

      if (close_cb)
        close_cb(rv, user_data);
    };

    std::promise<void> promise;
    std::future<void> future = promise.get_future();

    ctx->impl->CallSafe([&promise,
                         ctx,
                         result,
                         router = ctx->impl->router,
                         remotehost,
                         remoteport,
                         on_open = std::move(on_open),
                         on_close = std::move(on_close),
                         localAddr]() mutable {
      try
      {
        auto ep = ctx->endpoint();
        if (not ep)
          throw std::runtime_error{"lokinet_context->endpoint() returned null pointer."};
        auto* quic = ep->GetQUICTunnel();
        if (not quic)
          throw std::runtime_error{"lokinet_context endpoint has no quic tunnel manager."};

        auto [addr, id] =
            quic->open(remotehost, remoteport, std::move(on_open), std::move(on_close), localAddr);
        auto [host, port] = split_host_port(addr.ToString());
        result->tcp_id = id;
        tcp_okay(result, host, port, id);
        promise.set_value();
      }
      catch (...)
      {
        promise.set_exception(std::current_exception());
      }
    });

    try
    {
      future.get();
    }
    catch (std::invalid_argument& e)
    {
      llarp::log::error(logcat, "Error: exception caught: {}", e.what());
      tcp_error(result, EINVAL);
      return;
    }
    catch (std::runtime_error& e)
    {
      llarp::log::error(logcat, "Error: exception caught: {}", e.what());
      tcp_error(result, ENOTSUP);
      return;
    }
    catch (std::exception& e)
    {
      llarp::log::error(logcat, "Error: exception caught: {}", e.what());
      tcp_error(result, EBADF);
      return;
    }
    catch (...)
    {
      llarp::log::error(logcat, "Unknown exception caught.");
      tcp_error(result, EBADF);
      return;
    }

    ctx->outbound_tcp(remote, *result);
    assert(result->error == 0);
    return;
  }

  int EXPORT
  lokinet_inbound_tcp(uint16_t port, struct lokinet_context* ctx)
  {
    /// FIXME: delete pointer later
    return lokinet_inbound_tcp_filter(&accept_port, (void*)new std::uintptr_t{port}, ctx);
  }

  int EXPORT
  lokinet_inbound_tcp_filter(
      lokinet_tcp_filter acceptFilter, void* user, struct lokinet_context* ctx)
  {
    if (acceptFilter == nullptr)
    {
      acceptFilter = [](auto, auto, auto) { return 0; };
    }
    if (not ctx)
      return -1;
    std::promise<int> promise;
    {
      auto lock = ctx->acquire();
      if (not ctx->impl->IsUp())
      {
        return -1;
      }

      ctx->impl->CallSafe([ctx, acceptFilter, user, &promise]() {
        auto ep = ctx->endpoint();
        auto* quic = ep->GetQUICTunnel();
        auto id = quic->listen(
            [acceptFilter, user](auto remoteAddr, auto port) -> std::optional<llarp::SockAddr> {
              std::string remote{remoteAddr};
              if (auto result = acceptFilter(remote.c_str(), port, user))
              {
                if (result == -1)
                {
                  throw std::invalid_argument{"rejected"};
                }
              }
              else
                return llarp::SockAddr{"127.0.0.1:" + std::to_string(port)};
              return std::nullopt;
            });
        promise.set_value(id);
      });
    }
    auto ftr = promise.get_future();
    auto id = ftr.get();
    {
      auto lock = ctx->acquire();
      ctx->inbound_tcp(id);
    }
    return id;
  }

  char* EXPORT
  lokinet_hex_to_base32z(const char* hex)
  {
    std::string_view hexview{hex};
    if (not oxenc::is_hex(hexview))
      return nullptr;

    const size_t b32z_len = oxenc::to_base32z_size(oxenc::from_hex_size(hexview.size()));
    auto buf = std::make_unique<char[]>(b32z_len + 1);
    buf[b32z_len] = '\0';  // null terminate

    oxenc::hex_decoder decode{hexview.begin(), hexview.end()};
    oxenc::base32z_encoder encode{decode, decode.end()};
    std::copy(encode, encode.end(), buf.get());
    return buf.release();  // leak the buffer to the caller
  }

  void EXPORT
  lokinet_close_tcp(int tcp_id, struct lokinet_context* ctx)
  {
    if (not ctx)
      return;
    auto lock = ctx->acquire();
    if (not ctx->impl->IsUp())
      return;

    try
    {
      std::promise<void> promise;
      bool inbound = ctx->tcp_conns.at(tcp_id);
      ctx->impl->CallSafe([tcp_id, inbound, ctx, &promise]() {
        auto ep = ctx->endpoint();
        auto* quic = ep->GetQUICTunnel();
        try
        {
          if (inbound)
            quic->forget(tcp_id);
          else
            quic->close(tcp_id);
        }
        catch (...)
        {}
        promise.set_value();
      });
      for (auto& itr : ctx->active_conns)
      {
        if (itr.second.tcp_id == tcp_id)
          ctx->active_conns.erase(itr.first);
      }
      promise.get_future().get();
    }
    catch (...)
    {}
  }

  int EXPORT
  lokinet_srv_lookup(
      char* host,
      char* service,
      struct lokinet_srv_lookup_result* result,
      struct lokinet_context* ctx)
  {
    if (result == nullptr or ctx == nullptr or host == nullptr or service == nullptr)
      return -1;
    // sanity check, if the caller has not free()'d internals yet free them
    if (result->internal)
      delete result->internal;
    result->internal = new lokinet_srv_lookup_private{};
    return result->internal->LookupSRV(host, service, ctx);
  }

  void EXPORT
  lokinet_for_each_srv_record(
      struct lokinet_srv_lookup_result* result, lokinet_srv_record_iterator iter, void* user)
  {
    if (result and result->internal)
    {
      result->internal->IterateAll([iter, user](auto* result) { iter(result, user); });
    }
    else
    {
      iter(nullptr, user);
    }
  }

  void EXPORT
  lokinet_srv_lookup_done(struct lokinet_srv_lookup_result* result)
  {
    if (result == nullptr or result->internal == nullptr)
      return;
    delete result->internal;
    result->internal = nullptr;
  }

  int EXPORT
  lokinet_udp_bind(
      uint16_t exposedPort,
      lokinet_udp_flow_filter filter,
      lokinet_udp_flow_recv_func recv,
      lokinet_udp_flow_timeout_func timeout,
      void* user,
      struct lokinet_udp_bind_result* result,
      struct lokinet_context* ctx)
  {
    if (filter == nullptr or recv == nullptr or timeout == nullptr or result == nullptr
        or ctx == nullptr)
      return EINVAL;

    auto lock = ctx->acquire();
    if (auto ep = ctx->endpoint())
    {
      if (auto maybe = ctx->make_udp_handler(
              ep, llarp::net::port_t::from_host(exposedPort), filter, recv, timeout, user))
      {
        result->socket_id = *maybe;
        return 0;
      }
    }
    return EINVAL;
  }

  void EXPORT
  lokinet_udp_close(int socket_id, struct lokinet_context* ctx)
  {
    if (ctx)
    {
      ctx->remove_udp_handler(socket_id);
    }
  }

  int EXPORT
  lokinet_udp_flow_send(
      const struct lokinet_udp_flowinfo* remote,
      const void* ptr,
      size_t len,
      struct lokinet_context* ctx)
  {
    if (remote == nullptr or remote->remote_port == 0 or ptr == nullptr or len == 0
        or ctx == nullptr)
      return EINVAL;
    std::shared_ptr<llarp::EndpointBase> ep;
    llarp::nuint16_t srcport{0};
    auto dstport = llarp::net::port_t::from_host(remote->remote_port);
    {
      auto lock = ctx->acquire();
      if (auto itr = ctx->udp_sockets.find(remote->socket_id); itr != ctx->udp_sockets.end())
      {
        ep = itr->second->m_Endpoint.lock();
        srcport = itr->second->m_LocalPort;
      }
      else
        return EHOSTUNREACH;
    }
    if (auto maybe = llarp::service::ParseAddress(std::string{remote->remote_host}))
    {
      llarp::net::IPPacket pkt = llarp::net::IPPacket::UDP(
          llarp::nuint32_t{0},
          srcport,
          llarp::nuint32_t{0},
          dstport,
          llarp_buffer_t{reinterpret_cast<const uint8_t*>(ptr), len});

      if (pkt.empty())
        return EINVAL;
      std::promise<int> ret;
      ctx->impl->router->loop()->call([addr = *maybe, pkt = std::move(pkt), ep, &ret]() {
        if (auto tag = ep->GetBestConvoTagFor(addr); auto addr = ep->GetEndpointWithConvoTag(*tag))
        {
          if (ep->SendToOrQueue(
                  std::move(*addr), pkt.ConstBuffer(), llarp::service::ProtocolType::TrafficV4))
          {
            ret.set_value(0);
            return;
          }
        }
        ret.set_value(ENETUNREACH);
      });
      return ret.get_future().get();
    }
    return EINVAL;
  }

  int EXPORT
  lokinet_udp_establish(
      lokinet_udp_create_flow_func create_flow,
      void* user,
      const struct lokinet_udp_flowinfo* remote,
      struct lokinet_context* ctx)
  {
    if (create_flow == nullptr or remote == nullptr or ctx == nullptr)
      return EINVAL;
    std::shared_ptr<llarp::EndpointBase> ep;
    {
      auto lock = ctx->acquire();
      if (ctx->impl->router->loop()->inEventLoop())
      {
        llarp::LogError("cannot call udp_establish from internal event loop");
        return EINVAL;
      }
      if (auto itr = ctx->udp_sockets.find(remote->socket_id); itr != ctx->udp_sockets.end())
      {
        ep = itr->second->m_Endpoint.lock();
      }
      else
        return EHOSTUNREACH;
    }
    if (auto maybe = llarp::service::ParseAddress(std::string{remote->remote_host}))
    {
      {
        // check for pre existing flow
        auto lock = ctx->acquire();
        if (auto itr = ctx->udp_sockets.find(remote->socket_id); itr != ctx->udp_sockets.end())
        {
          auto& udp = itr->second;
          if (udp->m_Flows.count(*maybe))
          {
            // we already have a flow.
            return EADDRINUSE;
          }
        }
      }
      std::promise<bool> gotten;
      ctx->impl->router->loop()->call([addr = *maybe, ep, &gotten]() {
        ep->MarkAddressOutbound(addr);
        auto res = ep->EnsurePathTo(
            addr, [&gotten](auto result) { gotten.set_value(result.has_value()); }, 5s);
        if (not res)
        {
          gotten.set_value(false);
        }
      });
      if (gotten.get_future().get())
      {
        void* flow_data{nullptr};
        int flow_timeoutseconds{};
        create_flow(user, &flow_data, &flow_timeoutseconds);
        {
          auto lock = ctx->acquire();
          if (auto itr = ctx->udp_sockets.find(remote->socket_id); itr != ctx->udp_sockets.end())
          {
            itr->second->AddFlow(*maybe, *remote, flow_data, flow_timeoutseconds);
            return 0;
          }
          return EADDRINUSE;
        }
      }
      else
        return ETIMEDOUT;
    }
    return EINVAL;
  }

  void EXPORT
  lokinet_set_syncing_logger(lokinet_logger_func func, lokinet_logger_sync sync, void* user)
  {
    llarp::log::clear_sinks();
    llarp::log::add_sink(std::make_shared<llarp::logging::CallbackSink_mt>(func, sync, user));
  }

  void EXPORT
  lokinet_set_logger(lokinet_logger_func func, void* user)
  {
    lokinet_set_syncing_logger(func, nullptr, user);
  }
}
