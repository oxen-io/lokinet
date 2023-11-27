#pragma once

#include "route_poker.hpp"

#include <llarp/bootstrap.hpp>
#include <llarp/config/config.hpp>
#include <llarp/config/key_manager.hpp>
#include <llarp/consensus/reachability_testing.hpp>
#include <llarp/constants/link_layer.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/ev/ev.hpp>
#include <llarp/exit/context.hpp>
#include <llarp/handlers/tun.hpp>
#include <llarp/link/link_manager.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/profiling.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/rpc/lokid_rpc_client.hpp>
#include <llarp/rpc/rpc_server.hpp>
#include <llarp/service/context.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/fs.hpp>
#include <llarp/util/mem.hpp>
#include <llarp/util/service_manager.hpp>
#include <llarp/util/status.hpp>
#include <llarp/util/str.hpp>
#include <llarp/util/time.hpp>

#include <oxenmq/address.h>

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>

/*
  TONUKE:
    - hidden_service_context

  TODO:
    - router should hold DHT nodes container? in either a class or a map
    -
*/

namespace llarp
{
  /// number of routers to publish to
  static constexpr size_t INTROSET_RELAY_REDUNDANCY = 2;

  /// number of dht locations handled per relay
  static constexpr size_t INTROSET_REQS_PER_RELAY = 2;

  static constexpr size_t INTROSET_STORAGE_REDUNDANCY =
      (INTROSET_RELAY_REDUNDANCY * INTROSET_REQS_PER_RELAY);

  struct Contacts;

  struct Router : std::enable_shared_from_this<Router>
  {
    explicit Router(EventLoop_ptr loop, std::shared_ptr<vpn::Platform> vpnPlatform);

    ~Router();

   private:
    std::shared_ptr<RoutePoker> _route_poker;
    /// bootstrap RCs
    BootstrapList bootstrap_rc_list;
    std::chrono::steady_clock::time_point _next_explore_at;
    llarp_time_t last_pump = 0s;
    // transient iwp encryption key
    fs::path transport_keyfile;
    // long term identity key
    fs::path identity_keyfile;
    fs::path encryption_keyfile;
    // path to write our self signed rc to
    fs::path our_rc_file;
    // use file based logging?
    bool use_file_logging = false;
    // our router contact
    LocalRC router_contact;
    std::shared_ptr<oxenmq::OxenMQ> _lmq;
    path::BuildLimiter _pathbuild_limiter;
    std::shared_ptr<EventLoopWakeup> loop_wakeup;

    std::atomic<bool> is_stopping;
    std::atomic<bool> is_running;

    int _outbound_udp_socket = -1;
    bool _is_service_node = false;

    std::optional<SockAddr> _ourAddress;
    oxen::quic::Address _local_addr;

    EventLoop_ptr _loop;
    std::shared_ptr<vpn::Platform> _vpn;
    path::PathContext paths;
    exit::Context _exit_context;
    SecretKey _identity;
    SecretKey _encryption;
    std::shared_ptr<Contacts> _contacts;
    std::shared_ptr<NodeDB> _node_db;
    llarp_time_t _started_at;
    const oxenmq::TaggedThreadID _disk_thread;
    oxen::quic::Network _net;

    llarp_time_t _last_stats_report = 0s;
    llarp_time_t _next_decomm_warning = time_now_ms() + 15s;
    std::shared_ptr<llarp::KeyManager> _key_manager;
    std::shared_ptr<Config> _config;
    uint32_t _path_build_count = 0;

    std::unique_ptr<rpc::RPCServer> _rpc_server;

    const llarp_time_t _randomStartDelay;

    std::shared_ptr<rpc::LokidRpcClient> _rpc_client;
    bool whitelist_received{false};

    oxenmq::address rpc_addr;
    Profiling _router_profiling;
    fs::path _profile_file;
    LinkManager _link_manager{*this};
    std::chrono::system_clock::time_point last_rc_gossip{
        std::chrono::system_clock::time_point::min()};
    std::chrono::system_clock::time_point next_rc_gossip{
        std::chrono::system_clock::time_point::min()};

    // should we be sending padded messages every interval?
    bool send_padding = false;

    service::Context _hidden_service_context;

    consensus::reachability_testing router_testing;

    bool
    should_report_stats(llarp_time_t now) const;

    void
    report_stats();

    void
    save_rc();

    bool
    from_config(const Config& conf);

    bool
    insufficient_peers() const;

   public:
    void
    for_each_connection(std::function<void(link::Connection&)> func);

    void
    connect_to(const RouterID& rid);

    void
    connect_to(const RemoteRC& rc);

    Contacts*
    contacts() const
    {
      return _contacts.get();
    }

    std::shared_ptr<Config>
    config() const
    {
      return _config;
    }

    path::BuildLimiter&
    pathbuild_limiter()
    {
      return _pathbuild_limiter;
    }

    const llarp::net::Platform&
    net() const;

    const std::shared_ptr<oxenmq::OxenMQ>&
    lmq() const
    {
      return _lmq;
    }

    const std::shared_ptr<rpc::LokidRpcClient>&
    rpc_client() const
    {
      return _rpc_client;
    }

    LinkManager&
    link_manager()
    {
      return _link_manager;
    }

    inline int
    outbound_udp_socket() const
    {
      return _outbound_udp_socket;
    }

    exit::Context&
    exitContext()
    {
      return _exit_context;
    }

    const std::shared_ptr<KeyManager>&
    key_manager() const
    {
      return _key_manager;
    }

    const SecretKey&
    identity() const
    {
      return _identity;
    }

    const SecretKey&
    encryption() const
    {
      return _encryption;
    }

    Profiling&
    router_profiling()
    {
      return _router_profiling;
    }

    const EventLoop_ptr&
    loop() const
    {
      return _loop;
    }

    vpn::Platform*
    vpn_platform() const
    {
      return _vpn.get();
    }

    const std::shared_ptr<NodeDB>&
    node_db() const
    {
      return _node_db;
    }

    path::PathContext&
    path_context()
    {
      return paths;
    }

    const LocalRC&
    rc() const
    {
      return router_contact;
    }

    oxen::quic::Address
    public_ip() const;

    util::StatusObject
    ExtractStatus() const;

    util::StatusObject
    ExtractSummaryStatus() const;

    const std::unordered_set<RouterID>&
    get_whitelist() const;

    void
    set_router_whitelist(
        const std::vector<RouterID>& whitelist,
        const std::vector<RouterID>& greylist,
        const std::vector<RouterID>& unfunded);

    void
    queue_work(std::function<void(void)> func);

    void
    queue_disk_io(std::function<void(void)> func);

    /// Return true if we are operating as a service node and have received a service node whitelist
    bool
    have_snode_whitelist() const;

    /// return true if we look like we are a decommissioned service node
    bool
    appears_decommed() const;

    /// return true if we look like we are a registered, fully-staked service node (either active or
    /// decommissioned).  This condition determines when we are allowed to (and attempt to) connect
    /// to other peers when running as a service node.
    bool
    appears_funded() const;

    /// return true if we a registered service node; not that this only requires a partial stake,
    /// and does not imply that this service node is *active* or fully funded.
    bool
    appears_registered() const;

    /// return true if we look like we are allowed and able to test other routers
    bool
    can_test_routers() const;

    llarp_time_t
    Uptime() const;

    service::Context&
    hidden_service_context()
    {
      return _hidden_service_context;
    }

    const service::Context&
    hidden_service_context() const
    {
      return _hidden_service_context;
    }

    llarp_time_t _last_tick = 0s;

    std::function<void(void)> _router_close_cb;

    void
    set_router_close_cb(std::function<void(void)> hook)
    {
      _router_close_cb = hook;
    }

    bool
    LooksAlive() const
    {
      const llarp_time_t current = now();
      return current <= _last_tick || (current - _last_tick) <= llarp_time_t{30000};
    }

    const std::shared_ptr<RoutePoker>&
    route_poker() const
    {
      return _route_poker;
    }

    void
    TriggerPump();

    void
    PumpLL();

    std::string
    status_line();

    void
    InitInboundLinks();

    void
    InitOutboundLinks();

    std::optional<RouterID>
    GetRandomGoodRouter();

    /// initialize us as a service node
    /// return true on success
    bool
    init_service_node();

    bool
    IsRunning() const;

    /// return true if we are running in service node mode
    bool
    is_service_node() const;

    std::optional<std::string>
    OxendErrorState() const;

    void
    Close();

    bool
    Configure(std::shared_ptr<Config> conf, bool isSNode, std::shared_ptr<NodeDB> nodedb);

    bool
    StartRpcServer();

    void
    Freeze();

    void
    Thaw();

    bool
    Run();

    /// stop running the router logic gracefully
    void
    Stop();

    /// non graceful stop router
    void
    Die();

    /// close all sessions and shutdown all links
    void
    StopLinks();

    void
    persist_connection_until(const RouterID& remote, llarp_time_t until);

    bool
    EnsureIdentity();

    bool
    EnsureEncryptionKey();

    bool
    SessionToRouterAllowed(const RouterID& router) const;

    bool
    PathToRouterAllowed(const RouterID& router) const;

    /// return true if we are a client with an exit configured
    bool
    HasClientExit() const;

    const byte_t*
    pubkey() const
    {
      return seckey_to_pubkey(_identity);
    }

    /// send to remote router or queue for sending
    /// returns false on overflow
    /// returns true on successful queue
    /// NOT threadsafe
    /// MUST be called in the logic thread
    // bool
    // SendToOrQueue(
    //     const RouterID& remote, const AbstractLinkMessage& msg, SendStatusHandler handler);

    bool
    send_data_message(const RouterID& remote, std::string payload);

    bool
    send_control_message(
        const RouterID& remote,
        std::string endpoint,
        std::string body,
        std::function<void(oxen::quic::message m)> func = nullptr);

    bool IsBootstrapNode(RouterID) const;

    /// call internal router ticker
    void
    Tick();

    llarp_time_t
    now() const
    {
      return llarp::time_now_ms();
    }

    /// parse a routing message in a buffer and handle it with a handler if
    /// successful parsing return true on parse and handle success otherwise
    /// return false
    bool
    ParseRoutingMessageBuffer(
        const llarp_buffer_t& buf, path::AbstractHopHandler& p, const PathID_t& rxid);

    void
    ConnectToRandomRouters(int N);

    /// count the number of unique service nodes connected via pubkey
    size_t
    NumberOfConnectedRouters() const;

    /// count the number of unique clients connected by pubkey
    size_t
    NumberOfConnectedClients() const;

    bool
    GetRandomConnectedRouter(RemoteRC& result) const;

    bool
    HasSessionTo(const RouterID& remote) const;

    std::string
    ShortName() const;

    uint32_t
    NextPathBuildNumber();

    void
    AfterStopLinks();

    void
    AfterStopIssued();
  };
}  // namespace llarp
