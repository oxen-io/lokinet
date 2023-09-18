#pragma once

#include <llarp/bootstrap.hpp>
#include <llarp/config/config.hpp>
#include <llarp/config/key_manager.hpp>
#include <llarp/constants/link_layer.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/dht/context.hpp>
#include <llarp/ev/ev.hpp>
#include <llarp/exit/context.hpp>
#include <llarp/handlers/tun.hpp>
#include <llarp/link/link_manager.hpp>
#include <llarp/messages/link_message_parser.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/peerstats/peer_db.hpp>
#include <llarp/profiling.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/consensus/reachability_testing.hpp>
#include <llarp/tooling/router_event.hpp>
#include "outbound_message_handler.hpp"
#include "rc_gossiper.hpp"
#include "rc_lookup_handler.hpp"
#include "route_poker.hpp"
#include <llarp/routing/handler.hpp>
#include <llarp/routing/message_parser.hpp>
#include <llarp/rpc/lokid_rpc_client.hpp>
#include <llarp/rpc/rpc_server.hpp>
#include <llarp/service/context.hpp>
#include <stdexcept>
#include <llarp/util/buffer.hpp>
#include <llarp/util/fs.hpp>
#include <llarp/util/mem.hpp>
#include <llarp/util/status.hpp>
#include <llarp/util/str.hpp>
#include <llarp/util/time.hpp>
#include <llarp/util/service_manager.hpp>

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include <oxenmq/address.h>

namespace libquic = oxen::quic;

namespace llarp
{

  class RouteManager final /* : public Router */
  {
   public:
    std::shared_ptr<libquic::connection_interface>
    get_or_connect();

   private:
    std::shared_ptr<libquic::Endpoint> ep;
  };

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
    RouterContact router_contact;
    /// should we obey the service node whitelist?
    bool follow_whitelist = false;
    std::shared_ptr<oxenmq::OxenMQ> _lmq;
    path::BuildLimiter _pathbuild_limiter;
    std::shared_ptr<EventLoopWakeup> loop_wakeup;

    std::atomic<bool> is_stopping;
    std::atomic<bool> is_running;

    int _outbound_udp_socket = -1;
    bool is_service_node = false;

    std::optional<SockAddr> _ourAddress;
    oxen::quic::Address _local_addr;

    EventLoop_ptr _loop;
    std::shared_ptr<vpn::Platform> _vpn;
    path::PathContext paths;
    exit::Context _exit_context;
    SecretKey _identity;
    SecretKey _encryption;
    std::shared_ptr<dht::AbstractDHTMessageHandler> _dht;
    std::shared_ptr<NodeDB> _node_db;
    llarp_time_t _started_at;
    const oxenmq::TaggedThreadID _disk_thread;
    oxen::quic::Network _net;

    llarp_time_t _last_stats_report = 0s;
    llarp_time_t _next_decomm_warning = time_now_ms() + 15s;
    std::shared_ptr<llarp::KeyManager> _key_manager;
    std::shared_ptr<PeerDb> _peer_db;
    std::shared_ptr<Config> _config;
    uint32_t _path_build_count = 0;

    std::unique_ptr<rpc::RPCServer> m_RPCServer;

    const llarp_time_t _randomStartDelay;

    std::shared_ptr<rpc::LokidRpcClient> _rpc_client;

    oxenmq::address rpc_addr;
    Profiling _router_profiling;
    fs::path _profile_file;
    OutboundMessageHandler _outboundMessageHandler;
    LinkManager _link_manager{*this};
    RCLookupHandler _rc_lookup_handler;
    RCGossiper _rcGossiper;

    /// how often do we resign our RC? milliseconds.
    // TODO: make configurable
    llarp_time_t rc_regen_interval = 1h;

    // should we be sending padded messages every interval?
    bool send_padding = false;

    LinkMessageParser inbound_link_msg_parser;
    routing::InboundMessageParser inbound_routing_msg_parser;

    service::Context _hidden_service_context;

    consensus::reachability_testing router_testing;

    bool
    should_report_stats(llarp_time_t now) const;

    void
    report_stats();

    bool
    update_rc(bool rotateKeys = false);

    bool
    from_config(const Config& conf);

    void
    message_sent(const RouterID& remote, SendStatus status);

    bool
    insufficient_peers() const;

   protected:
    void
    handle_router_event(tooling::RouterEventPtr event) const;

    virtual bool
    disableGossipingRC_TestingOnly()
    {
      return false;
    };

   public:
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

    std::shared_ptr<dht::AbstractDHTMessageHandler>
    dht() const
    {
      return _dht;
    }

    // TOFIX: THIS
    OutboundMessageHandler&
    outboundMessageHandler()
    {
      return _outboundMessageHandler;
    }

    LinkManager&
    link_manager()
    {
      return _link_manager;
    }

    RCLookupHandler&
    rc_lookup_handler()
    {
      return _rc_lookup_handler;
    }

    std::shared_ptr<PeerDb>
    peer_db()
    {
      return _peer_db;
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

    const RouterContact&
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

    std::unordered_set<RouterID>
    router_whitelist() const
    {
      return _rc_lookup_handler.whitelist();
    }

    void
    modify_rc(std::function<std::optional<RouterContact>(RouterContact)> modify);

    void
    set_router_whitelist(
        const std::vector<RouterID>& whitelist,
        const std::vector<RouterID>& greylist,
        const std::vector<RouterID>& unfunded);

    template <class EventType, class... Params>
    void
    notify_router_event([[maybe_unused]] Params&&... args) const
    {
      // TODO: no-op when appropriate
      auto event = std::make_unique<EventType>(args...);
      handle_router_event(std::move(event));
    }

    void
    queue_work(std::function<void(void)> func);

    void
    queue_disk_io(std::function<void(void)> func);

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

    bool
    Sign(Signature& sig, const llarp_buffer_t& buf) const;

    service::Context&
    hiddenServiceContext()
    {
      return _hidden_service_context;
    }

    const service::Context&
    hiddenServiceContext() const
    {
      return _hidden_service_context;
    }

    llarp_time_t _lastTick = 0s;

    std::function<void(void)> _onDown;

    void
    SetDownHook(std::function<void(void)> hook)
    {
      _onDown = hook;
    }

    bool
    LooksAlive() const
    {
      const llarp_time_t now = Now();
      return now <= _lastTick || (now - _lastTick) <= llarp_time_t{30000};
    }

    const std::shared_ptr<RoutePoker>&
    routePoker() const
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
    GossipRCIfNeeded(const RouterContact rc);

    bool
    HandleRecvLinkMessageBuffer(AbstractLinkSession* from, const llarp_buffer_t& msg);

    void
    InitInboundLinks();

    void
    InitOutboundLinks();

    bool
    GetRandomGoodRouter(RouterID& r);

    /// initialize us as a service node
    /// return true on success
    bool
    InitServiceNode();

    bool
    IsRunning() const;

    /// return true if we are running in service node mode
    bool
    IsServiceNode() const;

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
    PersistSessionUntil(const RouterID& remote, llarp_time_t until);

    bool
    EnsureIdentity();

    bool
    EnsureEncryptionKey();

    bool
    SessionToRouterAllowed(const RouterID& router) const;

    bool
    PathToRouterAllowed(const RouterID& router) const;

    void
    HandleSaveRC() const;

    bool
    SaveRC();

    /// return true if we are a client with an exit configured
    bool
    HasClientExit() const;

    const byte_t*
    pubkey() const
    {
      return seckey_topublic(_identity);
    }

    void
    try_connect(fs::path rcfile);

    bool
    TryConnectAsync(RouterContact rc, uint16_t tries);

    /// send to remote router or queue for sending
    /// returns false on overflow
    /// returns true on successful queue
    /// NOT threadsafe
    /// MUST be called in the logic thread
    bool
    SendToOrQueue(
        const RouterID& remote, const AbstractLinkMessage& msg, SendStatusHandler handler);

    void
    ForEachPeer(
        std::function<void(const AbstractLinkSession*, bool)> visit, bool randomize = false) const;

    void
    ForEachPeer(std::function<void(AbstractLinkSession*)> visit);

    bool IsBootstrapNode(RouterID) const;

    /// check if newRc matches oldRC and update local rc for this remote contact
    /// if valid
    /// returns true on valid and updated
    /// returns false otherwise
    bool
    CheckRenegotiateValid(RouterContact newRc, RouterContact oldRC);

    /// called by link when a remote session has no more sessions open
    void
    SessionClosed(RouterID remote);

    /// called by link when an unestablished connection times out
    void
    ConnectionTimedOut(AbstractLinkSession* session);

    /// called by link when session is fully established
    bool
    ConnectionEstablished(AbstractLinkSession* session, bool inbound);

    /// call internal router ticker
    void
    Tick();

    llarp_time_t
    Now() const
    {
      return llarp::time_now_ms();
    }

    /// parse a routing message in a buffer and handle it with a handler if
    /// successful parsing return true on parse and handle success otherwise
    /// return false
    bool
    ParseRoutingMessageBuffer(
        const llarp_buffer_t& buf, routing::AbstractRoutingMessageHandler* h, const PathID_t& rxid);

    void
    ConnectToRandomRouters(int N);

    /// count the number of unique service nodes connected via pubkey
    size_t
    NumberOfConnectedRouters() const;

    /// count the number of unique clients connected by pubkey
    size_t
    NumberOfConnectedClients() const;

    bool
    GetRandomConnectedRouter(RouterContact& result) const;

    void
    HandleDHTLookupForExplore(RouterID remote, const std::vector<RouterContact>& results);

    void
    LookupRouter(RouterID remote, RouterLookupHandler resultHandler);

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
