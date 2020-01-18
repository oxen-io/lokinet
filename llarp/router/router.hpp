#ifndef LLARP_ROUTER_HPP
#define LLARP_ROUTER_HPP

#include <router/abstractrouter.hpp>

#include <bootstrap.hpp>
#include <config/key_manager.hpp>
#include <constants/link_layer.hpp>
#include <crypto/types.hpp>
#include <ev/ev.h>
#include <exit/context.hpp>
#include <handlers/tun.hpp>
#include <link/factory.hpp>
#include <link/link_manager.hpp>
#include <link/server.hpp>
#include <messages/link_message_parser.hpp>
#include <nodedb.hpp>
#include <path/path_context.hpp>
#include <profiling.hpp>
#include <router_contact.hpp>
#include <router/outbound_message_handler.hpp>
#include <router/outbound_session_maker.hpp>
#include <router/rc_lookup_handler.hpp>
#include <routing/handler.hpp>
#include <routing/message_parser.hpp>
#include <rpc/rpc.hpp>
#include <service/context.hpp>
#include <util/buffer.hpp>
#include <util/fs.hpp>
#include <util/mem.hpp>
#include <util/status.hpp>
#include <util/str.hpp>
#include <util/thread/logic.hpp>
#include <util/thread/threadpool.h>
#include <util/time.hpp>

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

namespace llarp
{
  struct Config;
}  // namespace llarp

namespace llarp
{
  struct Router final : public AbstractRouter
  {
    llarp_time_t _lastPump = 0;
    bool ready;
    // transient iwp encryption key
    fs::path transport_keyfile = "transport.key";

    // nodes to connect to on startup
    // DEPRECATED
    // std::map< std::string, fs::path > connect;

    // long term identity key
    fs::path ident_keyfile = "identity.key";

    fs::path encryption_keyfile = "encryption.key";

    // path to write our self signed rc to
    fs::path our_rc_file = "rc.signed";

    // use file based logging?
    bool m_UseFileLogging = false;

    // our router contact
    RouterContact _rc;

    /// are we using the lokid service node seed ?
    bool usingSNSeed = false;

    /// should we obey the service node whitelist?
    bool whitelistRouters = false;

    std::shared_ptr< Logic >
    logic() const override
    {
      return _logic;
    }

    llarp_dht_context *
    dht() const override
    {
      return _dht;
    }

    util::StatusObject
    ExtractStatus() const override;

    llarp_nodedb *
    nodedb() const override
    {
      return _nodedb;
    }

    const path::PathContext &
    pathContext() const override
    {
      return paths;
    }

    path::PathContext &
    pathContext() override
    {
      return paths;
    }

    const RouterContact &
    rc() const override
    {
      return _rc;
    }

    void
    SetRouterWhitelist(const std::vector< RouterID > &routers) override;

    exit::Context &
    exitContext() override
    {
      return _exitContext;
    }

    std::shared_ptr< KeyManager >
    keyManager() const override
    {
      return m_keyManager;
    }

    const SecretKey &
    identity() const override
    {
      return _identity;
    }

    const SecretKey &
    encryption() const override
    {
      return _encryption;
    }

    Profiling &
    routerProfiling() override
    {
      return _routerProfiling;
    }

    llarp_ev_loop_ptr
    netloop() const override
    {
      return _netloop;
    }

    std::shared_ptr< llarp::thread::ThreadPool >
    threadpool() override
    {
      return cryptoworker;
    }

    std::shared_ptr< llarp::thread::ThreadPool >
    diskworker() override
    {
      return disk;
    }

    // our ipv4 public setting
    bool publicOverride = false;
    struct sockaddr_in ip4addr;
    AddressInfo addrInfo;

    LinkFactory::LinkType _defaultLinkType;

    llarp_ev_loop_ptr _netloop;
    std::shared_ptr< llarp::thread::ThreadPool > cryptoworker;
    std::shared_ptr< Logic > _logic;
    path::PathContext paths;
    exit::Context _exitContext;
    SecretKey _identity;
    SecretKey _encryption;
    std::shared_ptr< thread::ThreadPool > disk;
    llarp_dht_context *_dht = nullptr;
    llarp_nodedb *_nodedb;
    llarp_time_t _startedAt;

    llarp_time_t
    Uptime() const override;

    bool
    Sign(Signature &sig, const llarp_buffer_t &buf) const override;

    uint16_t m_OutboundPort = 0;
    /// how often do we resign our RC? milliseconds.
    // TODO: make configurable
    llarp_time_t rcRegenInterval = 60 * 60 * 1000;

    // should we be sending padded messages every interval?
    bool sendPadding = false;

    uint32_t ticker_job_id = 0;

    LinkMessageParser inbound_link_msg_parser;
    routing::InboundMessageParser inbound_routing_msg_parser;

    service::Context _hiddenServiceContext;

    service::Context &
    hiddenServiceContext() override
    {
      return _hiddenServiceContext;
    }

    const service::Context &
    hiddenServiceContext() const override
    {
      return _hiddenServiceContext;
    }

    llarp_time_t _lastTick = 0;

    bool
    LooksAlive() const override
    {
      const llarp_time_t now = Now();
      return now <= _lastTick || (now - _lastTick) <= llarp_time_t{30000};
    }

    using NetConfig_t = std::unordered_multimap< std::string, std::string >;

    /// default network config for default network interface
    NetConfig_t netConfig;

    /// bootstrap RCs
    BootstrapList bootstrapRCList;

    bool
    ExitEnabled() const
    {
      // TODO: use equal_range ?
      auto itr = netConfig.find("exit");
      if(itr == netConfig.end())
        return false;
      return IsTrueValue(itr->second.c_str());
    }

    void
    PumpLL() override;

    bool
    CreateDefaultHiddenService();

    const std::string DefaultRPCBindAddr = "127.0.0.1:1190";
    bool enableRPCServer                 = false;
    std::unique_ptr< rpc::Server > rpcServer;
    std::string rpcBindAddr = DefaultRPCBindAddr;

    /// lokid caller
    std::unique_ptr< rpc::Caller > rpcCaller;
    std::string lokidRPCAddr = "127.0.0.1:22023";
    std::string lokidRPCUser;
    std::string lokidRPCPassword;

    Profiling _routerProfiling;
    std::string routerProfilesFile = "profiles.dat";

    OutboundMessageHandler _outboundMessageHandler;
    OutboundSessionMaker _outboundSessionMaker;
    LinkManager _linkManager;
    RCLookupHandler _rcLookupHandler;

    using Clock_t     = std::chrono::steady_clock;
    using TimePoint_t = Clock_t::time_point;

    TimePoint_t m_NextExploreAt;

    IOutboundMessageHandler &
    outboundMessageHandler() override
    {
      return _outboundMessageHandler;
    }

    IOutboundSessionMaker &
    outboundSessionMaker() override
    {
      return _outboundSessionMaker;
    }

    ILinkManager &
    linkManager() override
    {
      return _linkManager;
    }

    I_RCLookupHandler &
    rcLookupHandler() override
    {
      return _rcLookupHandler;
    }

    Router(std::shared_ptr< llarp::thread::ThreadPool > worker,
           llarp_ev_loop_ptr __netloop, std::shared_ptr< Logic > logic);

    ~Router() override;

    bool
    HandleRecvLinkMessageBuffer(ILinkSession *from,
                                const llarp_buffer_t &msg) override;

    bool
    InitOutboundLinks();

    bool
    GetRandomGoodRouter(RouterID &r) override;

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
    LoadHiddenServiceConfig(string_view fname);

    bool
    AddHiddenService(const service::Config::section_t &config);

    bool
    Configure(Config *conf, llarp_nodedb *nodedb = nullptr) override;

    bool
    StartJsonRpc() override;

    bool
    Run() override;

    /// stop running the router logic gracefully
    void
    Stop() override;

    /// close all sessions and shutdown all links
    void
    StopLinks();

    void
    PersistSessionUntil(const RouterID &remote, llarp_time_t until) override;

    bool
    EnsureIdentity();

    bool
    EnsureEncryptionKey();

    bool
    ConnectionToRouterAllowed(const RouterID &router) const override;

    void
    HandleSaveRC() const;

    bool
    SaveRC();

    const byte_t *
    pubkey() const override
    {
      return seckey_topublic(_identity);
    }

    void
    try_connect(fs::path rcfile);

    /// inject configuration and reconfigure router
    bool
    Reconfigure(Config *conf) override;

    bool
    TryConnectAsync(RouterContact rc, uint16_t tries) override;

    /// validate new configuration against old one
    /// return true on 100% valid
    /// return false if not 100% valid
    bool
    ValidateConfig(Config *conf) const override;

    /// send to remote router or queue for sending
    /// returns false on overflow
    /// returns true on successful queue
    /// NOT threadsafe
    /// MUST be called in the logic thread
    bool
    SendToOrQueue(const RouterID &remote, const ILinkMessage *msg,
                  SendStatusHandler handler) override;

    void
    ForEachPeer(std::function< void(const ILinkSession *, bool) > visit,
                bool randomize = false) const override;

    void
    ForEachPeer(std::function< void(ILinkSession *) > visit);

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
    ScheduleTicker(uint64_t i = 1000);

    /// parse a routing message in a buffer and handle it with a handler if
    /// successful parsing return true on parse and handle success otherwise
    /// return false
    bool
    ParseRoutingMessageBuffer(const llarp_buffer_t &buf,
                              routing::IMessageHandler *h,
                              const PathID_t &rxid) override;

    void
    ConnectToRandomRouters(int N) override;

    /// count the number of unique service nodes connected via pubkey
    size_t
    NumberOfConnectedRouters() const override;

    /// count the number of unique clients connected by pubkey
    size_t
    NumberOfConnectedClients() const override;

    bool
    GetRandomConnectedRouter(RouterContact &result) const override;

    void
    HandleDHTLookupForExplore(
        RouterID remote, const std::vector< RouterContact > &results) override;

    void
    LookupRouter(RouterID remote, RouterLookupHandler resultHandler) override;

    bool
    HasSessionTo(const RouterID &remote) const override;

    void
    handle_router_ticker();

    void
    AfterStopLinks();

    void
    AfterStopIssued();

   private:
    std::atomic< bool > _stopping;
    std::atomic< bool > _running;

    bool m_isServiceNode = false;

    llarp_time_t m_LastStatsReport = 0;

    std::shared_ptr< llarp::KeyManager > m_keyManager;

    bool
    ShouldReportStats(llarp_time_t now) const;

    void
    ReportStats();

    bool
    UpdateOurRC(bool rotateKeys = false);

    template < typename Config >
    void
    mergeHiddenServiceConfig(const Config &in, Config &out)
    {
      for(const auto &item : netConfig)
        out.push_back({item.first, item.second});
      for(const auto &item : in)
        out.push_back({item.first, item.second});
    }

    bool
    FromConfig(Config *conf);

    void
    MessageSent(const RouterID &remote, SendStatus status);
  };

}  // namespace llarp

#endif
