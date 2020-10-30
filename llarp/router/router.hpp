#ifndef LLARP_ROUTER_HPP
#define LLARP_ROUTER_HPP

#include <router/abstractrouter.hpp>

#include <bootstrap.hpp>
#include <config/config.hpp>
#include <config/key_manager.hpp>
#include <constants/link_layer.hpp>
#include <crypto/types.hpp>
#include <ev/ev.h>
#include <exit/context.hpp>
#include <handlers/tun.hpp>
#include <link/link_manager.hpp>
#include <link/server.hpp>
#include <messages/link_message_parser.hpp>
#include <nodedb.hpp>
#include <path/path_context.hpp>
#include <peerstats/peer_db.hpp>
#include <profiling.hpp>
#include <router_contact.hpp>
#include <router/outbound_message_handler.hpp>
#include <router/outbound_session_maker.hpp>
#include <router/rc_gossiper.hpp>
#include <router/rc_lookup_handler.hpp>
#include <router/route_poker.hpp>
#include <routing/handler.hpp>
#include <routing/message_parser.hpp>
#include <rpc/lokid_rpc_client.hpp>
#include <rpc/rpc_server.hpp>
#include <service/context.hpp>
#include <stdexcept>
#include <util/buffer.hpp>
#include <util/fs.hpp>
#include <util/mem.hpp>
#include <util/status.hpp>
#include <util/str.hpp>
#include <util/thread/logic.hpp>
#include <util/time.hpp>

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include <lokimq/address.h>

namespace llarp
{
  struct Router : public AbstractRouter
  {
    llarp_time_t _lastPump = 0s;
    bool ready;
    // transient iwp encryption key
    fs::path transport_keyfile;

    // long term identity key
    fs::path ident_keyfile;

    fs::path encryption_keyfile;

    // path to write our self signed rc to
    fs::path our_rc_file;

    // use file based logging?
    bool m_UseFileLogging = false;

    // our router contact
    RouterContact _rc;

    /// should we obey the service node whitelist?
    bool whitelistRouters = false;

    LMQ_ptr m_lmq;

    LMQ_ptr
    lmq() const override
    {
      return m_lmq;
    }

    std::shared_ptr<rpc::LokidRpcClient>
    RpcClient() const override
    {
      return m_lokidRpcClient;
    }

    std::shared_ptr<Logic>
    logic() const override
    {
      return _logic;
    }

    llarp_dht_context*
    dht() const override
    {
      return _dht;
    }

    util::StatusObject
    ExtractStatus() const override;

    llarp_nodedb*
    nodedb() const override
    {
      return _nodedb;
    }

    const path::PathContext&
    pathContext() const override
    {
      return paths;
    }

    path::PathContext&
    pathContext() override
    {
      return paths;
    }

    const RouterContact&
    rc() const override
    {
      return _rc;
    }

    void
    SetRouterWhitelist(const std::vector<RouterID> routers) override;

    exit::Context&
    exitContext() override
    {
      return _exitContext;
    }

    std::shared_ptr<KeyManager>
    keyManager() const override
    {
      return m_keyManager;
    }

    const SecretKey&
    identity() const override
    {
      return _identity;
    }

    const SecretKey&
    encryption() const override
    {
      return _encryption;
    }

    Profiling&
    routerProfiling() override
    {
      return _routerProfiling;
    }

    llarp_ev_loop_ptr
    netloop() const override
    {
      return _netloop;
    }

    void
    QueueWork(std::function<void(void)> func) override;

    void
    QueueDiskIO(std::function<void(void)> func) override;

    IpAddress _ourAddress;

    llarp_ev_loop_ptr _netloop;
    std::shared_ptr<Logic> _logic;
    path::PathContext paths;
    exit::Context _exitContext;
    SecretKey _identity;
    SecretKey _encryption;
    llarp_dht_context* _dht = nullptr;
    llarp_nodedb* _nodedb;
    llarp_time_t _startedAt;
    const lokimq::TaggedThreadID m_DiskThread;

    llarp_time_t
    Uptime() const override;

    bool
    Sign(Signature& sig, const llarp_buffer_t& buf) const override;

    uint16_t m_OutboundPort = 0;
    /// how often do we resign our RC? milliseconds.
    // TODO: make configurable
    llarp_time_t rcRegenInterval = 1h;

    // should we be sending padded messages every interval?
    bool sendPadding = false;

    uint32_t ticker_job_id = 0;

    LinkMessageParser inbound_link_msg_parser;
    routing::InboundMessageParser inbound_routing_msg_parser;

    service::Context _hiddenServiceContext;

    service::Context&
    hiddenServiceContext() override
    {
      return _hiddenServiceContext;
    }

    const service::Context&
    hiddenServiceContext() const override
    {
      return _hiddenServiceContext;
    }

    llarp_time_t _lastTick = 0s;

    std::function<void(void)> _onDown;

    void
    SetDownHook(std::function<void(void)> hook) override
    {
      _onDown = hook;
    }

    bool
    LooksAlive() const override
    {
      const llarp_time_t now = Now();
      return now <= _lastTick || (now - _lastTick) <= llarp_time_t{30000};
    }

    /// bootstrap RCs
    BootstrapList bootstrapRCList;

    bool
    ExitEnabled() const
    {
      return false;  // FIXME - have to fix the FIXME because FIXME
      throw std::runtime_error("FIXME: this needs to be derived from config");
      /*
      // TODO: use equal_range ?
      auto itr = netConfig.find("exit");
      if (itr == netConfig.end())
        return false;
      return IsTrueValue(itr->second.c_str());
      */
    }

    RoutePoker&
    routePoker() override
    {
      return m_RoutePoker;
    }

    RoutePoker m_RoutePoker;

    void
    PumpLL() override;

    const lokimq::address DefaultRPCBindAddr = lokimq::address::tcp("127.0.0.1", 1190);
    bool enableRPCServer = false;
    lokimq::address rpcBindAddr = DefaultRPCBindAddr;
    std::unique_ptr<rpc::RpcServer> m_RPCServer;

    const llarp_time_t _randomStartDelay;

    std::shared_ptr<rpc::LokidRpcClient> m_lokidRpcClient;

    lokimq::address lokidRPCAddr;

    Profiling _routerProfiling;
    std::string routerProfilesFile = "profiles.dat";

    OutboundMessageHandler _outboundMessageHandler;
    OutboundSessionMaker _outboundSessionMaker;
    LinkManager _linkManager;
    RCLookupHandler _rcLookupHandler;
    RCGossiper _rcGossiper;

    using Clock_t = std::chrono::steady_clock;
    using TimePoint_t = Clock_t::time_point;

    TimePoint_t m_NextExploreAt;

    IOutboundMessageHandler&
    outboundMessageHandler() override
    {
      return _outboundMessageHandler;
    }

    IOutboundSessionMaker&
    outboundSessionMaker() override
    {
      return _outboundSessionMaker;
    }

    ILinkManager&
    linkManager() override
    {
      return _linkManager;
    }

    I_RCLookupHandler&
    rcLookupHandler() override
    {
      return _rcLookupHandler;
    }

    std::shared_ptr<PeerDb>
    peerDb() override
    {
      return m_peerDb;
    }

    void
    GossipRCIfNeeded(const RouterContact rc) override;

    explicit Router(llarp_ev_loop_ptr __netloop, std::shared_ptr<Logic> logic);

    virtual ~Router() override;

    bool
    HandleRecvLinkMessageBuffer(ILinkSession* from, const llarp_buffer_t& msg) override;

    bool
    InitOutboundLinks();

    bool
    GetRandomGoodRouter(RouterID& r) override;

    /// initialize us as a service node
    /// return true on success
    bool
    InitServiceNode();

    bool
    IsRunning() const override;

    /// return true if we are running in service node mode
    bool
    IsServiceNode() const override;

    void
    Close();

    bool
    Configure(std::shared_ptr<Config> conf, bool isRouter, llarp_nodedb* nodedb = nullptr) override;

    bool
    StartRpcServer() override;

    bool
    Run() override;

    /// stop running the router logic gracefully
    void
    Stop() override;

    /// non graceful stop router
    void
    Die() override;

    /// close all sessions and shutdown all links
    void
    StopLinks();

    void
    PersistSessionUntil(const RouterID& remote, llarp_time_t until) override;

    bool
    EnsureIdentity();

    bool
    EnsureEncryptionKey();

    bool
    ConnectionToRouterAllowed(const RouterID& router) const override;

    void
    HandleSaveRC() const;

    bool
    SaveRC();

    /// return true if we are a client with an exit configured
    bool
    HasClientExit() const;

    const byte_t*
    pubkey() const override
    {
      return seckey_topublic(_identity);
    }

    void
    try_connect(fs::path rcfile);

    bool
    TryConnectAsync(RouterContact rc, uint16_t tries) override;

    /// send to remote router or queue for sending
    /// returns false on overflow
    /// returns true on successful queue
    /// NOT threadsafe
    /// MUST be called in the logic thread
    bool
    SendToOrQueue(
        const RouterID& remote, const ILinkMessage* msg, SendStatusHandler handler) override;

    void
    ForEachPeer(std::function<void(const ILinkSession*, bool)> visit, bool randomize = false)
        const override;

    void
    ForEachPeer(std::function<void(ILinkSession*)> visit);

    bool IsBootstrapNode(RouterID) const override;

    /// check if newRc matches oldRC and update local rc for this remote contact
    /// if valid
    /// returns true on valid and updated
    /// returns false otherwise
    bool
    CheckRenegotiateValid(RouterContact newRc, RouterContact oldRC) override;

    /// called by link when a remote session has no more sessions open
    void
    SessionClosed(RouterID remote) override;

    /// called by link when an unestablished connection times out
    void
    ConnectionTimedOut(ILinkSession* session);

    /// called by link when session is fully established
    bool
    ConnectionEstablished(ILinkSession* session, bool inbound);

    /// call internal router ticker
    void
    Tick();

    llarp_time_t
    Now() const override
    {
      return llarp::time_now_ms();
    }

    /// schedule ticker to call i ms from now
    void
    ScheduleTicker(llarp_time_t i = 1s);

    /// parse a routing message in a buffer and handle it with a handler if
    /// successful parsing return true on parse and handle success otherwise
    /// return false
    bool
    ParseRoutingMessageBuffer(
        const llarp_buffer_t& buf, routing::IMessageHandler* h, const PathID_t& rxid) override;

    void
    ConnectToRandomRouters(int N) override;

    /// count the number of unique service nodes connected via pubkey
    size_t
    NumberOfConnectedRouters() const override;

    /// count the number of unique clients connected by pubkey
    size_t
    NumberOfConnectedClients() const override;

    bool
    GetRandomConnectedRouter(RouterContact& result) const override;

    void
    HandleDHTLookupForExplore(RouterID remote, const std::vector<RouterContact>& results) override;

    void
    LookupRouter(RouterID remote, RouterLookupHandler resultHandler) override;

    bool
    HasSessionTo(const RouterID& remote) const override;

    std::string
    ShortName() const override;

    uint32_t
    NextPathBuildNumber() override;

    void
    handle_router_ticker();

    void
    AfterStopLinks();

    void
    AfterStopIssued();

    std::shared_ptr<Config> m_Config;

    std::shared_ptr<Config>
    GetConfig() const override
    {
      return m_Config;
    }

   private:
    std::atomic<bool> _stopping;
    std::atomic<bool> _running;

    bool m_isServiceNode = false;

    llarp_time_t m_LastStatsReport = 0s;

    std::shared_ptr<llarp::KeyManager> m_keyManager;
    std::shared_ptr<PeerDb> m_peerDb;

    uint32_t path_build_count = 0;

    bool
    ShouldReportStats(llarp_time_t now) const;

    void
    ReportStats();

    bool
    UpdateOurRC(bool rotateKeys = false);

    bool
    FromConfig(const Config& conf);

    void
    MessageSent(const RouterID& remote, SendStatus status);

   protected:
    virtual void
    HandleRouterEvent(tooling::RouterEventPtr event) const override;

    virtual bool
    disableGossipingRC_TestingOnly()
    {
      return false;
    };
  };

}  // namespace llarp

#endif
