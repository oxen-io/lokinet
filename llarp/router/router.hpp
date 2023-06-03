#pragma once

#include "abstractrouter.hpp"

#include <llarp/bootstrap.hpp>
#include <llarp/config/config.hpp>
#include <llarp/config/key_manager.hpp>
#include <llarp/constants/link_layer.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/ev/ev.hpp>
#include <llarp/exit/context.hpp>
#include <llarp/handlers/tun.hpp>
#include <llarp/link/link_manager.hpp>
#include <llarp/link/server.hpp>
#include <llarp/messages/link_message_parser.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/peerstats/peer_db.hpp>
#include <llarp/profiling.hpp>
#include <llarp/router_contact.hpp>
#include "outbound_message_handler.hpp"
#include "outbound_session_maker.hpp"
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

    path::BuildLimiter m_PathBuildLimiter;

    std::shared_ptr<EventLoopWakeup> m_Pump;
    std::shared_ptr<EventLoopWakeup> m_Work;
    std::vector<std::function<void()>> m_WorkJobs;

    /// submits cpu heavy work from last event loop tick cycle to worker threads.
    void
    submit_work();

    path::BuildLimiter&
    pathBuildLimiter() override
    {
      return m_PathBuildLimiter;
    }

    const llarp::net::Platform&
    Net() const override;

    const LMQ_ptr&
    lmq() const override
    {
      return m_lmq;
    }

    const std::shared_ptr<rpc::LokidRpcClient>&
    RpcClient() const override
    {
      return m_lokidRpcClient;
    }

    llarp_dht_context*
    dht() const override
    {
      return _dht;
    }

    std::optional<std::variant<nuint32_t, nuint128_t>>
    OurPublicIP() const override;

    util::StatusObject
    ExtractStatus() const override;

    util::StatusObject
    ExtractSummaryStatus() const override;

    const std::shared_ptr<NodeDB>&
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
    ModifyOurRC(std::function<std::optional<RouterContact>(RouterContact)> modify) override;

    void
    SetRouterWhitelist(
        const std::vector<RouterID>& whitelist,
        const std::vector<RouterID>& greylist,
        const std::vector<RouterID>& unfunded) override;

    std::unordered_set<RouterID>
    GetRouterWhitelist() const override
    {
      return _rcLookupHandler.Whitelist();
    }

    exit::Context&
    exitContext() override
    {
      return _exitContext;
    }

    const std::shared_ptr<KeyManager>&
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

    const EventLoop_ptr&
    loop() const override
    {
      return _loop;
    }

    vpn::Platform*
    GetVPNPlatform() const override
    {
      return _vpnPlatform.get();
    }

    /// queue functionally pure cpu heavy work to be done in another thread.
    void
    QueueWork(std::function<void(void)> func) override;

    /// queue disk io bound work to be done in the disk io thread.
    void
    QueueDiskIO(std::function<void(void)> func) override;

    /// return true if we look like we are a decommissioned service node
    bool
    LooksDecommissioned() const;

    /// return true if we look like we are a registered, fully-staked service node (either active or
    /// decommissioned).  This condition determines when we are allowed to (and attempt to) connect
    /// to other peers when running as a service node.
    bool
    LooksFunded() const;

    /// return true if we a registered service node; not that this only requires a partial stake,
    /// and does not imply that this service node is *active* or fully funded.
    bool
    LooksRegistered() const;

    /// return true if we look like we are allowed and able to test other routers
    bool
    ShouldTestOtherRouters() const;

    std::optional<SockAddr> _ourAddress;

    EventLoop_ptr _loop;
    std::shared_ptr<vpn::Platform> _vpnPlatform;
    path::PathContext paths;
    exit::Context _exitContext;
    SecretKey _identity;
    SecretKey _encryption;
    llarp_dht_context* _dht = nullptr;
    std::shared_ptr<NodeDB> _nodedb;
    llarp_time_t _startedAt;
    const oxenmq::TaggedThreadID m_DiskThread;

    llarp_time_t
    Uptime() const override;

    bool
    Sign(Signature& sig, const llarp_buffer_t& buf) const override;

    /// how often do we resign our RC? milliseconds.
    // TODO: make configurable
    llarp_time_t rcRegenInterval = 1h;

    // should we be sending padded messages every interval?
    bool sendPadding = false;

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

    const std::shared_ptr<RoutePoker>&
    routePoker() const override
    {
      return m_RoutePoker;
    }

    std::shared_ptr<RoutePoker> m_RoutePoker;

    void
    TriggerPump() override;

    void
    PumpLL();

    std::unique_ptr<rpc::RPCServer> m_RPCServer;

    const llarp_time_t _randomStartDelay;

    std::shared_ptr<rpc::LokidRpcClient> m_lokidRpcClient;

    oxenmq::address lokidRPCAddr;
    Profiling _routerProfiling;
    fs::path _profilesFile;
    OutboundMessageHandler _outboundMessageHandler;
    OutboundSessionMaker _outboundSessionMaker;
    LinkManager _linkManager;
    RCLookupHandler _rcLookupHandler;
    RCGossiper _rcGossiper;

    std::string
    status_line() override;

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

    inline int
    OutboundUDPSocket() const override
    {
      return m_OutboundUDPSocket;
    }

    void
    GossipRCIfNeeded(const RouterContact rc) override;

    explicit Router(EventLoop_ptr loop, std::shared_ptr<vpn::Platform> vpnPlatform);

    ~Router() override;

    bool
    HandleRecvLinkMessageBuffer(ILinkSession* from, const llarp_buffer_t& msg) override;

    void
    InitInboundLinks();

    void
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

    std::optional<std::string>
    OxendErrorState() const override;

    void
    Close();

    bool
    Configure(std::shared_ptr<Config> conf, bool isSNode, std::shared_ptr<NodeDB> nodedb) override;

    bool
    StartRpcServer() override;

    void
    Freeze() override;

    void
    Thaw() override;

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
    SessionToRouterAllowed(const RouterID& router) const override;
    bool
    PathToRouterAllowed(const RouterID& router) const override;

    void
    HandleSaveRC() const;

    bool
    SaveRC();

    /// return true if we are a client with an exit configured
    bool
    HasClientExit() const override;

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
        const RouterID& remote, const ILinkMessage& msg, SendStatusHandler handler) override;

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
    AfterStopLinks();

    void
    AfterStopIssued();

    std::shared_ptr<Config> m_Config;

    std::shared_ptr<Config>
    GetConfig() const override
    {
      return m_Config;
    }

    int m_OutboundUDPSocket = -1;

   private:
    std::atomic<bool> _stopping;
    std::atomic<bool> _running;

    bool m_isServiceNode = false;

    // Delay warning about being decommed/dereged until we've had enough time to sync up with oxend
    static constexpr auto DECOMM_WARNING_STARTUP_DELAY = 15s;

    llarp_time_t m_LastStatsReport = 0s;
    llarp_time_t m_NextDecommissionWarn = time_now_ms() + DECOMM_WARNING_STARTUP_DELAY;
    std::shared_ptr<llarp::KeyManager> m_keyManager;
    std::shared_ptr<PeerDb> m_peerDb;

    uint32_t path_build_count = 0;

    consensus::reachability_testing m_routerTesting;

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

    bool
    TooFewPeers() const;

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
