#include <memory>
#include <router/router.hpp>

#include <config/config.hpp>
#include <constants/proto.hpp>
#include <constants/files.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <crypto/crypto.hpp>
#include <dht/context.hpp>
#include <dht/node.hpp>
#include <iwp/iwp.hpp>
#include <link/server.hpp>
#include <messages/link_message.hpp>
#include <net/net.hpp>
#include <rpc/rpc.hpp>
#include <stdexcept>
#include <util/buffer.hpp>
#include <util/encode.hpp>
#include <util/logging/file_logger.hpp>
#include <util/logging/json_logger.hpp>
#include <util/logging/logger_syslog.hpp>
#include <util/logging/logger.hpp>
#include <util/meta/memfn.hpp>
#include <util/str.hpp>
#include <ev/ev.hpp>

#include "tooling/router_event.hpp"

#include <fstream>
#include <cstdlib>
#include <iterator>
#include <unordered_map>
#include <utility>
#if defined(ANDROID) || defined(IOS)
#include <unistd.h>
#endif

#if defined(WITH_SYSTEMD)
#include <systemd/sd-daemon.h>
#endif

static constexpr std::chrono::milliseconds ROUTER_TICK_INTERVAL = 1s;

namespace llarp
{
  Router::Router(
      std::shared_ptr<llarp::thread::ThreadPool> _tp,
      llarp_ev_loop_ptr __netloop,
      std::shared_ptr<Logic> l)
      : ready(false)
      , _netloop(std::move(__netloop))
      , cryptoworker(std::move(_tp))
      , _logic(std::move(l))
      , paths(this)
      , _exitContext(this)
      , disk(std::make_shared<llarp::thread::ThreadPool>(1, 1000, "diskworker"))
      , _dht(llarp_dht_context_new(this))
      , inbound_link_msg_parser(this)
      , _hiddenServiceContext(this)
#ifdef LOKINET_HIVE
      , _randomStartDelay(std::chrono::milliseconds((llarp::randint() % 1250) + 2000))
#else
      , _randomStartDelay(std::chrono::seconds((llarp::randint() % 30) + 10))
#endif
  {
    m_keyManager = std::make_shared<KeyManager>();

    _stopping.store(false);
    _running.store(false);
    _lastTick = llarp::time_now_ms();
    m_NextExploreAt = Clock_t::now();
  }

  Router::~Router()
  {
    llarp_dht_context_free(_dht);
  }

  util::StatusObject
  Router::ExtractStatus() const
  {
    if (_running)
    {
      return util::StatusObject{{"running", true},
                                {"numNodesKnown", _nodedb->num_loaded()},
                                {"dht", _dht->impl->ExtractStatus()},
                                {"services", _hiddenServiceContext.ExtractStatus()},
                                {"exit", _exitContext.ExtractStatus()},
                                {"links", _linkManager.ExtractStatus()},
                                {"outboundMessages", _outboundMessageHandler.ExtractStatus()}};
    }
    else
    {
      return util::StatusObject{{"running", false}};
    }
  }

  bool
  Router::HandleRecvLinkMessageBuffer(ILinkSession* session, const llarp_buffer_t& buf)
  {
    if (_stopping)
      return true;

    if (!session)
    {
      LogWarn("no link session");
      return false;
    }
    return inbound_link_msg_parser.ProcessFrom(session, buf);
  }

  void
  Router::PersistSessionUntil(const RouterID& remote, llarp_time_t until)
  {
    _linkManager.PersistSessionUntil(remote, until);
  }

  void
  Router::GossipRCIfNeeded(const RouterContact rc)
  {
    /// if we are not a service node forget about gossip
    if (not IsServiceNode())
      return;
    /// wait for random uptime
    if (std::chrono::milliseconds{Uptime()} < _randomStartDelay)
      return;
    _rcGossiper.GossipRC(rc);
  }

  bool
  Router::GetRandomGoodRouter(RouterID& router)
  {
    if (whitelistRouters)
    {
      return _rcLookupHandler.GetRandomWhitelistRouter(router);
    }

    auto pick_router = [&](auto& collection) -> bool {
      const auto sz = collection.size();
      auto itr = collection.begin();
      if (sz == 0)
        return false;
      if (sz > 1)
        std::advance(itr, randint() % sz);
      router = itr->first;
      return true;
    };

    std::shared_lock l{nodedb()->access};
    return pick_router(nodedb()->entries);
  }

  void
  Router::PumpLL()
  {
    const auto now = Now();
    if (_stopping.load())
      return;
    paths.PumpDownstream();
    paths.PumpUpstream();
    _outboundMessageHandler.Tick();
    _linkManager.PumpLinks();
  }

  bool
  Router::SendToOrQueue(const RouterID& remote, const ILinkMessage* msg, SendStatusHandler handler)
  {
    return _outboundMessageHandler.QueueMessage(remote, msg, handler);
  }

  void
  Router::ForEachPeer(std::function<void(const ILinkSession*, bool)> visit, bool randomize) const
  {
    _linkManager.ForEachPeer(visit, randomize);
  }

  void
  Router::ForEachPeer(std::function<void(ILinkSession*)> visit)
  {
    _linkManager.ForEachPeer(visit);
  }

  void
  Router::try_connect(fs::path rcfile)
  {
    RouterContact remote;
    if (!remote.Read(rcfile.string().c_str()))
    {
      LogError("failure to decode or verify of remote RC");
      return;
    }
    if (remote.Verify(Now()))
    {
      LogDebug("verified signature");
      _outboundSessionMaker.CreateSessionTo(remote, nullptr);
    }
    else
      LogError(rcfile, " contains invalid RC");
  }

  bool
  Router::EnsureIdentity()
  {
    if (whitelistRouters)
    {
#if defined(ANDROID) || defined(IOS)
      LogError("running a service node on mobile device is not possible.");
      return false;
#else
#if defined(_WIN32)
      LogError("running a service node on windows is not possible.");
      return false;
#endif
#endif
    }

    _identity = m_keyManager->identityKey;
    _encryption = m_keyManager->encryptionKey;

    if (_identity.IsZero())
      return false;
    if (_encryption.IsZero())
      return false;

    return true;
  }

  bool
  Router::Configure(Config* conf, bool isRouter, llarp_nodedb* nodedb)
  {
    if (nodedb == nullptr)
    {
      throw std::invalid_argument("nodedb cannot be null");
    }
    _nodedb = nodedb;

    if (not m_keyManager->initialize(*conf, true, isRouter))
      throw std::runtime_error("KeyManager failed to initialize");

    if (!FromConfig(conf))
      throw std::runtime_error("FromConfig() failed");

    if (!InitOutboundLinks())
      throw std::runtime_error("InitOutboundLinks() failed");

    if (not EnsureIdentity())
      throw std::runtime_error("EnsureIdentity() failed");

    return true;
  }

  /// called in disk worker thread
  void
  Router::HandleSaveRC() const
  {
    std::string fname = our_rc_file.string();
    _rc.Write(fname.c_str());
  }

  bool
  Router::SaveRC()
  {
    LogDebug("verify RC signature");
    if (!_rc.Verify(Now()))
    {
      Dump<MAX_RC_SIZE>(rc());
      LogError("RC is invalid, not saving");
      return false;
    }
    diskworker()->addJob(std::bind(&Router::HandleSaveRC, this));
    return true;
  }

  bool
  Router::IsServiceNode() const
  {
    return m_isServiceNode;
  }

  void
  Router::Close()
  {
    LogInfo("closing router");
    llarp_ev_loop_stop(_netloop);
    disk->stop();
    disk->shutdown();
    _running.store(false);
  }

  void
  Router::handle_router_ticker()
  {
    ticker_job_id = 0;
    Tick();
    ScheduleTicker(ROUTER_TICK_INTERVAL);
  }

  bool
  Router::ParseRoutingMessageBuffer(
      const llarp_buffer_t& buf, routing::IMessageHandler* h, const PathID_t& rxid)
  {
    return inbound_routing_msg_parser.ParseMessageBuffer(buf, h, rxid, this);
  }

  bool
  Router::ConnectionToRouterAllowed(const RouterID& router) const
  {
    return _rcLookupHandler.RemoteIsAllowed(router);
  }

  size_t
  Router::NumberOfConnectedRouters() const
  {
    return _linkManager.NumberOfConnectedRouters();
  }

  size_t
  Router::NumberOfConnectedClients() const
  {
    return _linkManager.NumberOfConnectedClients();
  }

  bool
  Router::UpdateOurRC(bool rotateKeys)
  {
    SecretKey nextOnionKey;
    RouterContact nextRC = _rc;
    if (rotateKeys)
    {
      CryptoManager::instance()->encryption_keygen(nextOnionKey);
      std::string f = encryption_keyfile.string();
      // TODO: use disk worker
      if (nextOnionKey.SaveToFile(f.c_str()))
      {
        nextRC.enckey = seckey_topublic(nextOnionKey);
        _encryption = nextOnionKey;
      }
    }
    if (!nextRC.Sign(identity()))
      return false;
    if (!nextRC.Verify(time_now_ms(), false))
      return false;
    _rc = std::move(nextRC);
    if (rotateKeys)
    {
      // propagate RC by renegotiating sessions
      ForEachPeer([](ILinkSession* s) {
        if (s->RenegotiateSession())
          LogInfo("renegotiated session");
        else
          LogWarn("failed to renegotiate session");
      });
    }
    if (IsServiceNode())
      return SaveRC();
    else
      return true;
  }

  bool
  Router::FromConfig(Config* conf)
  {
    // Set netid before anything else
    if (!conf->router.m_netId.empty() && strcmp(conf->router.m_netId.c_str(), llarp::DEFAULT_NETID))
    {
      const auto& netid = conf->router.m_netId;
      llarp::LogWarn(
          "!!!! you have manually set netid to be '",
          netid,
          "' which does not equal '",
          llarp::DEFAULT_NETID,
          "' you will run as a different network, good luck "
          "and don't forget: something something MUH traffic "
          "shape correlation !!!!");
      NetID::DefaultValue() = NetID(reinterpret_cast<const byte_t*>(netid.c_str()));
      // reset netid in our rc
      _rc.netID = llarp::NetID();
    }

    // IWP config
    m_OutboundPort = conf->links.m_OutboundLink.port;
    // Router config
    _rc.SetNick(conf->router.m_nickname);
    _outboundSessionMaker.maxConnectedRouters = conf->router.m_maxConnectedRouters;
    _outboundSessionMaker.minConnectedRouters = conf->router.m_minConnectedRouters;

    encryption_keyfile = m_keyManager->m_encKeyPath;
    our_rc_file = m_keyManager->m_rcPath;
    transport_keyfile = m_keyManager->m_transportKeyPath;
    ident_keyfile = m_keyManager->m_idKeyPath;

    _ourAddress = conf->router.m_publicAddress;

    RouterContact::BlockBogons = conf->router.m_blockBogons;

    // Lokid Config
    usingSNSeed = conf->lokid.usingSNSeed;
    whitelistRouters = conf->lokid.whitelistRouters;
    lokidRPCAddr = IpAddress(conf->lokid.lokidRPCAddr);  // TODO: make config's option an IpAddress
    lokidRPCUser = conf->lokid.lokidRPCUser;
    lokidRPCPassword = conf->lokid.lokidRPCPassword;

    if (usingSNSeed)
      ident_keyfile = conf->lokid.ident_keyfile;

    // TODO: add config flag for "is service node"
    if (conf->links.m_InboundLinks.size())
    {
      m_isServiceNode = true;
    }

    networkConfig = conf->network;

    /// build a set of  strictConnectPubkeys (
    /// TODO: make this consistent with config -- do we support multiple strict connections
    //        or not?
    std::set<RouterID> strictConnectPubkeys;
    if (not networkConfig.m_strictConnect.empty())
    {
      const auto& val = networkConfig.m_strictConnect;
      if (IsServiceNode())
        throw std::runtime_error("cannot use strict-connect option as service node");

      // try as a RouterID and as a PubKey, convert to RouterID if needed
      llarp::RouterID snode;
      llarp::PubKey pk;
      if (pk.FromString(val))
        strictConnectPubkeys.emplace(pk);
      else if (snode.FromString(val))
        strictConnectPubkeys.insert(snode);
      else
        throw std::invalid_argument(stringify("invalid key for strict-connect: ", val));
    }

    std::vector<fs::path> configRouters = conf->connect.routers;
    configRouters.insert(
        configRouters.end(), conf->bootstrap.routers.begin(), conf->bootstrap.routers.end());

    // if our conf had no bootstrap files specified, try the default location of
    // <DATA_DIR>/bootstrap.signed. If this isn't present, leave a useful error message
    if (configRouters.size() == 0 and not m_isServiceNode)
    {
      // TODO: use constant
      fs::path defaultBootstrapFile = conf->router.m_dataDir / "bootstrap.signed";
      if (fs::exists(defaultBootstrapFile))
        configRouters.push_back(defaultBootstrapFile);
      else
      {
        LogError("No bootstrap files specified in config file, and the default");
        LogError("bootstrap file ", defaultBootstrapFile, " does not exist.");
        LogError("Please provide a bootstrap file (e.g. run 'lokinet-bootstrap)'");
        throw std::runtime_error("No bootstrap files available.");
      }
    }

    BootstrapList b_list;
    for (const auto& router : configRouters)
    {
      bool isListFile = false;
      {
        std::ifstream inf(router.c_str(), std::ios::binary);
        if (inf.is_open())
        {
          const char ch = inf.get();
          isListFile = ch == 'l';
        }
      }
      if (isListFile)
      {
        if (not BDecodeReadFile(router, b_list))
        {
          throw std::runtime_error(stringify("failed to read bootstrap list file '", router, "'"));
        }
      }
      else
      {
        RouterContact rc;
        if (not rc.Read(router))
        {
          throw std::runtime_error(
              stringify("failed to decode bootstrap RC, file='", router, "' rc=", rc));
        }
        b_list.insert(rc);
      }
    }

    for (auto& rc : b_list)
    {
      if (not rc.Verify(Now()))
      {
        LogWarn("ignoring invalid RC: ", RouterID(rc.pubkey));
        continue;
      }
      bootstrapRCList.emplace(std::move(rc));
    }

    LogInfo("Loaded ", bootstrapRCList.size(), " bootstrap routers");

    // Init components after relevant config settings loaded
    _outboundMessageHandler.Init(&_linkManager, _logic);
    _outboundSessionMaker.Init(
        &_linkManager, &_rcLookupHandler, &_routerProfiling, _logic, _nodedb, threadpool());
    _linkManager.Init(&_outboundSessionMaker);
    _rcLookupHandler.Init(
        _dht,
        _nodedb,
        threadpool(),
        &_linkManager,
        &_hiddenServiceContext,
        strictConnectPubkeys,
        bootstrapRCList,
        whitelistRouters,
        m_isServiceNode);

    // create inbound links, if we are a service node
    for (const LinksConfig::LinkInfo& serverConfig : conf->links.m_InboundLinks)
    {
      auto server = iwp::NewInboundLink(
          m_keyManager,
          util::memFn(&AbstractRouter::rc, this),
          util::memFn(&AbstractRouter::HandleRecvLinkMessageBuffer, this),
          util::memFn(&AbstractRouter::Sign, this),
          util::memFn(&IOutboundSessionMaker::OnSessionEstablished, &_outboundSessionMaker),
          util::memFn(&AbstractRouter::CheckRenegotiateValid, this),
          util::memFn(&IOutboundSessionMaker::OnConnectTimeout, &_outboundSessionMaker),
          util::memFn(&AbstractRouter::SessionClosed, this),
          util::memFn(&AbstractRouter::PumpLL, this));

      const std::string& key = serverConfig.interface;
      int af = serverConfig.addressFamily;
      uint16_t port = serverConfig.port;
      if (!server->Configure(netloop(), key, af, port))
      {
        throw std::runtime_error(stringify("failed to bind inbound link on ", key, " port ", port));
      }
      _linkManager.AddLink(std::move(server), true);
    }

    // Network config
    if (conf->network.m_enableProfiling.has_value() and not*conf->network.m_enableProfiling)
    {
      routerProfiling().Disable();
      LogWarn("router profiling explicitly disabled");
    }

    if (!conf->network.m_routerProfilesFile.empty())
    {
      routerProfilesFile = conf->network.m_routerProfilesFile;
      routerProfiling().Load(routerProfilesFile.c_str());
      llarp::LogInfo("setting profiles to ", routerProfilesFile);
    }

    // API config
    enableRPCServer = conf->api.m_enableRPCServer;
    rpcBindAddr = IpAddress(conf->api.m_rpcBindAddr);  // TODO: make this an IpAddress in config
    if (not IsServiceNode())
    {
      hiddenServiceContext().AddEndpoint(*conf);
    }

    // Logging config
    LogContext::Instance().Initialize(
        conf->logging.m_logLevel,
        conf->logging.m_logType,
        conf->logging.m_logFile,
        conf->router.m_nickname,
        diskworker());

    return true;
  }

  bool
  Router::CheckRenegotiateValid(RouterContact newrc, RouterContact oldrc)
  {
    return _rcLookupHandler.CheckRenegotiateValid(newrc, oldrc);
  }

  bool
  Router::IsBootstrapNode(const RouterID r) const
  {
    return std::count_if(
               bootstrapRCList.begin(),
               bootstrapRCList.end(),
               [r](const RouterContact& rc) -> bool { return rc.pubkey == r; })
        > 0;
  }

  bool
  Router::ShouldReportStats(llarp_time_t now) const
  {
    static constexpr auto ReportStatsInterval = 1h;
    return now - m_LastStatsReport > ReportStatsInterval;
  }

  void
  Router::ReportStats()
  {
    const auto now = Now();
    LogInfo(nodedb()->num_loaded(), " RCs loaded");
    LogInfo(bootstrapRCList.size(), " bootstrap peers");
    LogInfo(NumberOfConnectedRouters(), " router connections");
    if (IsServiceNode())
    {
      LogInfo(NumberOfConnectedClients(), " client connections");
      LogInfo(_rc.Age(now), " since we last updated our RC");
      LogInfo(_rc.TimeUntilExpires(now), " until our RC expires");
    }
    LogInfo(now - m_LastStatsReport, " last reported stats");
    m_LastStatsReport = now;
  }

  void
  Router::Tick()
  {
    if (_stopping)
      return;
    // LogDebug("tick router");
    const auto now = Now();

#if defined(WITH_SYSTEMD)
    {
      std::stringstream ss;
      ss << "WATCHDOG=1\nSTATUS=v" << llarp::VERSION_STR;
      if (IsServiceNode())
      {
        ss << " snode | known/svc/clients: " << nodedb()->num_loaded() << "/"
           << NumberOfConnectedRouters() << "/" << NumberOfConnectedClients() << " | "
           << pathContext().CurrentTransitPaths() << " active paths";
      }
      else
      {
        ss << " client | known/connected: " << nodedb()->num_loaded() << "/"
           << NumberOfConnectedRouters() << " | path success: ";
        hiddenServiceContext().ForEachService([&ss](const auto& name, const auto& ep) {
          ss << " [" << name << " " << std::setprecision(4)
             << (100.0 * ep->CurrentBuildStats().SuccessRatio()) << "%]";
          return true;
        });
      }
      const auto status = ss.str();
      ::sd_notify(0, status.c_str());
    }
#endif

    routerProfiling().Tick();

    if (ShouldReportStats(now))
    {
      ReportStats();
    }

    _rcGossiper.Decay(now);

    _rcLookupHandler.PeriodicUpdate(now);

    const bool isSvcNode = IsServiceNode();

    if (_rc.ExpiresSoon(now, std::chrono::milliseconds(randint() % 10000))
        || (now - _rc.last_updated) > rcRegenInterval)
    {
      LogInfo("regenerating RC");
      if (!UpdateOurRC(false))
        LogError("Failed to update our RC");
    }
    else
    {
      GossipRCIfNeeded(_rc);
    }
    const bool gotWhitelist = _rcLookupHandler.HaveReceivedWhitelist();
    // remove RCs for nodes that are no longer allowed by network policy
    nodedb()->RemoveIf([&](const RouterContact& rc) -> bool {
      // don't purge bootstrap nodes from nodedb
      if (IsBootstrapNode(rc.pubkey))
        return false;
      // if for some reason we stored an RC that isn't a valid router
      // purge this entry
      if (not rc.IsPublicRouter())
        return true;
      // clients have a notion of a whilelist
      // we short circuit logic here so we dont remove
      // routers that are not whitelisted for first hops
      if (not isSvcNode)
        return false;
      // if we have a whitelist enabled and we don't
      // have the whitelist yet don't remove the entry
      if (whitelistRouters and not gotWhitelist)
        return false;
      // if we have no whitelist enabled or we have
      // the whitelist enabled and we got the whitelist
      // check against the whitelist and remove if it's not
      // in the whitelist OR if there is no whitelist don't remove
      return not _rcLookupHandler.RemoteIsAllowed(rc.pubkey);
    });

    _linkManager.CheckPersistingSessions(now);

    size_t connected = NumberOfConnectedRouters();
    if (not isSvcNode)
    {
      connected += _linkManager.NumberOfPendingConnections();
    }

    const int interval = isSvcNode ? 5 : 2;
    const auto timepoint_now = Clock_t::now();
    if (timepoint_now >= m_NextExploreAt)
    {
      _rcLookupHandler.ExploreNetwork();
      m_NextExploreAt = timepoint_now + std::chrono::seconds(interval);
    }
    size_t connectToNum = _outboundSessionMaker.minConnectedRouters;
    const auto strictConnect = _rcLookupHandler.NumberOfStrictConnectRouters();
    if (strictConnect > 0 && connectToNum > strictConnect)
    {
      connectToNum = strictConnect;
    }

    if (connected < connectToNum)
    {
      size_t dlt = connectToNum - connected;
      LogInfo("connecting to ", dlt, " random routers to keep alive");
      _outboundSessionMaker.ConnectToRandomRouters(dlt);
    }

    _hiddenServiceContext.Tick(now);
    _exitContext.Tick(now);

    if (rpcCaller)
      rpcCaller->Tick(now);
    // save profiles
    if (routerProfiling().ShouldSave(now))
    {
      diskworker()->addJob([&]() { routerProfiling().Save(routerProfilesFile.c_str()); });
    }
    // save nodedb
    if (nodedb()->ShouldSaveToDisk(now))
    {
      nodedb()->AsyncFlushToDisk();
    }

    // get connected peers
    std::set<dht::Key_t> peersWeHave;
    _linkManager.ForEachPeer([&peersWeHave](ILinkSession* s) {
      if (!s->IsEstablished())
        return;
      peersWeHave.emplace(s->GetPubKey());
    });
    // remove any nodes we don't have connections to
    _dht->impl->Nodes()->RemoveIf(
        [&peersWeHave](const dht::Key_t& k) -> bool { return peersWeHave.count(k) == 0; });
    // expire paths
    paths.ExpirePaths(now);
    // update tick timestamp
    _lastTick = llarp::time_now_ms();
  }

  bool
  Router::Sign(Signature& sig, const llarp_buffer_t& buf) const
  {
    return CryptoManager::instance()->sign(sig, identity(), buf);
  }

  void
  Router::ScheduleTicker(llarp_time_t interval)
  {
    ticker_job_id = _logic->call_later(interval, std::bind(&Router::handle_router_ticker, this));
  }

  void
  Router::SessionClosed(RouterID remote)
  {
    dht::Key_t k(remote);
    dht()->impl->Nodes()->DelNode(k);

    LogInfo("Session to ", remote, " fully closed");
  }

  bool
  Router::GetRandomConnectedRouter(RouterContact& result) const
  {
    return _linkManager.GetRandomConnectedRouter(result);
  }

  void
  Router::HandleDHTLookupForExplore(RouterID /*remote*/, const std::vector<RouterContact>& results)
  {
    for (const auto& rc : results)
    {
      _rcLookupHandler.CheckRC(rc);
    }
  }

  // TODO: refactor callers and remove this function
  void
  Router::LookupRouter(RouterID remote, RouterLookupHandler resultHandler)
  {
    _rcLookupHandler.GetRC(
        remote,
        [=](const RouterID& id, const RouterContact* const rc, const RCRequestResult result) {
          (void)id;
          if (resultHandler)
          {
            std::vector<RouterContact> routers;
            if (result == RCRequestResult::Success && rc != nullptr)
            {
              routers.push_back(*rc);
            }
            resultHandler(routers);
          }
        });
  }

  void
  Router::SetRouterWhitelist(const std::vector<RouterID>& routers)
  {
    _rcLookupHandler.SetRouterWhitelist(routers);
  }

  bool
  Router::StartJsonRpc()
  {
    if (_running || _stopping)
      return false;

    if (enableRPCServer)
    {
      if (rpcBindAddr.isEmpty())
      {
        rpcBindAddr = DefaultRPCBindAddr;
      }
      rpcServer = std::make_unique<rpc::Server>(this);
      if (not rpcServer->Start(rpcBindAddr))
      {
        LogError("failed to bind jsonrpc to ", rpcBindAddr);
        return false;
      }
      LogInfo("Bound RPC server to ", rpcBindAddr);
    }

    return true;
  }

  bool
  Router::Run()
  {
    if (_running || _stopping)
      return false;

    if (whitelistRouters)
    {
      rpcCaller = std::make_unique<rpc::Caller>(this);
      rpcCaller->SetAuth(lokidRPCUser, lokidRPCPassword);
      if (not rpcCaller->Start(lokidRPCAddr))
      {
        LogError("RPC Caller to ", lokidRPCAddr, " failed to start");
        return false;
      }
      LogInfo("RPC Caller to ", lokidRPCAddr, " started");
    }

    if (!cryptoworker->start())
    {
      LogError("crypto worker failed to start");
      return false;
    }

    if (!disk->start())
    {
      LogError("disk worker failed to start");
      return false;
    }

    routerProfiling().Load(routerProfilesFile.c_str());

    // set public signing key
    _rc.pubkey = seckey_topublic(identity());
    // set router version if service node
    if (IsServiceNode())
    {
      _rc.routerVersion = RouterVersion(llarp::VERSION, LLARP_PROTO_VERSION);
    }

    _linkManager.ForEachInboundLink([&](LinkLayer_ptr link) {
      AddressInfo ai;
      if (link->GetOurAddressInfo(ai))
      {
        // override ip and port
        if (not _ourAddress.isEmpty())
        {
          ai.fromIpAddress(_ourAddress);
        }
        if (RouterContact::BlockBogons && IsBogon(ai.ip))
          return;
        LogInfo("adding address: ", ai);
        _rc.addrs.push_back(ai);
        if (ExitEnabled())
        {
          const IpAddress address = ai.toIpAddress();
          _rc.exits.emplace_back(_rc.pubkey, address);
          LogInfo("Exit relay started, advertised as exiting at: ", address);
        }
      }
    });

    // set public encryption key
    _rc.enckey = seckey_topublic(encryption());

    LogInfo("Signing rc...");
    if (!_rc.Sign(identity()))
    {
      LogError("failed to sign rc");
      return false;
    }

    if (IsServiceNode())
    {
      if (!SaveRC())
      {
        LogError("failed to save RC");
        return false;
      }
    }
    _outboundSessionMaker.SetOurRouter(pubkey());
    if (!_linkManager.StartLinks(_logic, cryptoworker))
    {
      LogWarn("One or more links failed to start.");
      return false;
    }

    if (IsServiceNode())
    {
      // initialize as service node
      if (!InitServiceNode())
      {
        LogError("Failed to initialize service node");
        return false;
      }
      const RouterID us = pubkey();
      LogInfo("initalized service node: ", us);
      // init gossiper here
      _rcGossiper.Init(&_linkManager, us, this);
      // relays do not use profiling
      routerProfiling().Disable();
    }
    else
    {
      // we are a client
      // regenerate keys and resign rc before everything else
      CryptoManager::instance()->identity_keygen(_identity);
      CryptoManager::instance()->encryption_keygen(_encryption);
      _rc.pubkey = seckey_topublic(identity());
      _rc.enckey = seckey_topublic(encryption());
      if (!_rc.Sign(identity()))
      {
        LogError("failed to regenerate keys and sign RC");
        return false;
      }
    }

    LogInfo("starting hidden service context...");
    if (!hiddenServiceContext().StartAll())
    {
      LogError("Failed to start hidden service context");
      return false;
    }

    {
      ssize_t loaded = _nodedb->LoadAll();
      llarp::LogInfo("loaded ", loaded, " RCs");
      if (loaded < 0)
      {
        // shouldn't be possible
        return false;
      }
    }

    llarp_dht_context_start(dht(), pubkey());

    for (const auto& rc : bootstrapRCList)
    {
      if (this->nodedb()->Insert(rc))
      {
        LogInfo("added bootstrap node ", RouterID(rc.pubkey));
      }
      else
      {
        LogError("Failed to add bootstrap node ", RouterID(rc.pubkey));
      }
      _dht->impl->Nodes()->PutNode(rc);
    }

    LogInfo("have ", _nodedb->num_loaded(), " routers");

    _netloop->add_ticker(std::bind(&Router::PumpLL, this));

    ScheduleTicker(ROUTER_TICK_INTERVAL);
    _running.store(true);
    _startedAt = Now();
#if defined(WITH_SYSTEMD)
    ::sd_notify(0, "READY=1");
#endif
    LogContext::Instance().DropToRuntimeLevel();
    return _running;
  }

  bool
  Router::IsRunning() const
  {
    return _running;
  }

  llarp_time_t
  Router::Uptime() const
  {
    const llarp_time_t _now = Now();
    if (_startedAt > 0s && _now > _startedAt)
      return _now - _startedAt;
    return 0s;
  }

  void
  Router::AfterStopLinks()
  {
    Close();
  }

  void
  Router::AfterStopIssued()
  {
    StopLinks();
    nodedb()->AsyncFlushToDisk();
    _logic->call_later(200ms, std::bind(&Router::AfterStopLinks, this));
  }

  void
  Router::StopLinks()
  {
    _linkManager.Stop();
  }

  void
  Router::Stop()
  {
    if (!_running)
      return;
    if (_stopping)
      return;

    _stopping.store(true);
    LogContext::Instance().RevertRuntimeLevel();
    LogInfo("stopping router");
#if defined(WITH_SYSTEMD)
    sd_notify(0, "STOPPING=1\nSTATUS=Shutting down");
#endif
    hiddenServiceContext().StopAll();
    _exitContext.Stop();
    if (rpcServer)
      rpcServer->Stop();
    paths.PumpUpstream();
    _linkManager.PumpLinks();
    _logic->call_later(200ms, std::bind(&Router::AfterStopIssued, this));
  }

  bool
  Router::HasSessionTo(const RouterID& remote) const
  {
    return _linkManager.HasSessionTo(remote);
  }

  std::string
  Router::ShortName() const
  {
    return RouterID(pubkey()).ToString().substr(0, 8);
  }

  uint32_t
  Router::NextPathBuildNumber()
  {
    return path_build_count++;
  }

  void
  Router::ConnectToRandomRouters(int _want)
  {
    const size_t want = _want;
    auto connected = NumberOfConnectedRouters();
    if (not IsServiceNode())
    {
      connected += _linkManager.NumberOfPendingConnections();
    }
    if (connected >= want)
      return;
    _outboundSessionMaker.ConnectToRandomRouters(want);
  }

  bool
  Router::InitServiceNode()
  {
    LogInfo("accepting transit traffic");
    paths.AllowTransit();
    llarp_dht_allow_transit(dht());
    _exitContext.AddExitEndpoint("default-connectivity", networkConfig, dnsConfig);
    return true;
  }

  bool
  Router::ValidateConfig(Config* /*conf*/) const
  {
    return true;
  }

  bool
  Router::Reconfigure(Config*)
  {
    // TODO: implement me
    return true;
  }

  bool
  Router::TryConnectAsync(RouterContact rc, uint16_t tries)
  {
    (void)tries;

    if (rc.pubkey == pubkey())
    {
      return false;
    }

    if (!_rcLookupHandler.RemoteIsAllowed(rc.pubkey))
    {
      return false;
    }

    _outboundSessionMaker.CreateSessionTo(rc, nullptr);

    return true;
  }

  bool
  Router::InitOutboundLinks()
  {
    auto link = iwp::NewOutboundLink(
        m_keyManager,
        util::memFn(&AbstractRouter::rc, this),
        util::memFn(&AbstractRouter::HandleRecvLinkMessageBuffer, this),
        util::memFn(&AbstractRouter::Sign, this),
        util::memFn(&IOutboundSessionMaker::OnSessionEstablished, &_outboundSessionMaker),
        util::memFn(&AbstractRouter::CheckRenegotiateValid, this),
        util::memFn(&IOutboundSessionMaker::OnConnectTimeout, &_outboundSessionMaker),
        util::memFn(&AbstractRouter::SessionClosed, this),
        util::memFn(&AbstractRouter::PumpLL, this));

    if (!link)
      throw std::runtime_error("NewOutboundLink() failed to provide a link");

    const auto afs = {AF_INET, AF_INET6};

    for (const auto af : afs)
    {
      if (not link->Configure(netloop(), "*", af, m_OutboundPort))
        continue;

      _linkManager.AddLink(std::move(link), false);
      return true;
    }
    throw std::runtime_error(
        stringify("Failed to init AF_INET and AF_INET6 on port ", m_OutboundPort));
  }

  void
  Router::MessageSent(const RouterID& remote, SendStatus status)
  {
    if (status == SendStatus::Success)
    {
      LogDebug("Message successfully sent to ", remote);
    }
    else
    {
      LogDebug("Message failed sending to ", remote);
    }
  }
}  // namespace llarp
