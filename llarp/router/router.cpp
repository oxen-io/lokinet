#include "router.hpp"

#include <llarp/nodedb.hpp>
#include <llarp/config/config.hpp>
#include <llarp/constants/proto.hpp>
#include <llarp/constants/time.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/dht/node.hpp>
#include <llarp/ev/ev.hpp>
#include <llarp/link/contacts.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/net/net.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/util/meta/memfn.hpp>
#include <llarp/util/status.hpp>
#include <memory>
#include <cstdlib>
#include <iterator>
#include <unordered_map>
#include <utility>
#include <stdexcept>
#if defined(ANDROID) || defined(IOS)
#include <unistd.h>
#endif

#if defined(WITH_SYSTEMD)
#include <systemd/sd-daemon.h>
#endif

#include <llarp/constants/platform.hpp>
#include <oxenmq/oxenmq.h>

static constexpr std::chrono::milliseconds ROUTER_TICK_INTERVAL = 250ms;

namespace llarp
{
  static auto logcat = log::Cat("router");

  Router::Router(EventLoop_ptr loop, std::shared_ptr<vpn::Platform> vpnPlatform)
      : _route_poker{std::make_shared<RoutePoker>(*this)}
      , _lmq{std::make_shared<oxenmq::OxenMQ>()}
      , _loop{std::move(loop)}
      , _vpn{std::move(vpnPlatform)}
      , paths{this}
      , _exit_context{this}
      , _disk_thread{_lmq->add_tagged_thread("disk")}
      , _rpc_server{nullptr}
      , _randomStartDelay{platform::is_simulation ? std::chrono::milliseconds{(llarp::randint() % 1250) + 2000} : 0s}
      , _link_manager{*this}
      , _hidden_service_context{this}
  {
    _key_manager = std::make_shared<KeyManager>();
    // for lokid, so we don't close the connection when syncing the whitelist
    _lmq->MAX_MSG_SIZE = -1;
    is_stopping.store(false);
    is_running.store(false);
    _last_tick = llarp::time_now_ms();
    _next_explore_at = std::chrono::steady_clock::now();
    loop_wakeup = _loop->make_waker([this]() { PumpLL(); });
  }

  Router::~Router()
  {
    _contacts.reset();
  }

  // TODO: investigate changes needed for libquic integration
  //       still needed at all?

  // TODO: No. The answer is No.
  // TONUKE: EVERYTHING ABOUT THIS
  void
  Router::PumpLL()
  {
    llarp::LogTrace("Router::PumpLL() start");
    if (is_stopping.load())
      return;
    paths.PumpDownstream();
    paths.PumpUpstream();
    _hidden_service_context.Pump();
    llarp::LogTrace("Router::PumpLL() end");
  }

  // TOFIX: this
  util::StatusObject
  Router::ExtractStatus() const
  {
    if (not is_running)
      util::StatusObject{{"running", false}};

    return util::StatusObject{
        {"running", true},
        {"numNodesKnown", _node_db->num_loaded()},
        {"contacts", _contacts->ExtractStatus()},
        {"services", _hidden_service_context.ExtractStatus()},
        {"exit", _exit_context.ExtractStatus()},
        {"links", _link_manager.extract_status()},
        /* {"outboundMessages", _outboundMessageHandler.ExtractStatus()} */};
  }

  // TODO: investigate changes needed for libquic integration
  util::StatusObject
  Router::ExtractSummaryStatus() const
  {
    if (!is_running)
      return util::StatusObject{{"running", false}};

    auto services = _hidden_service_context.ExtractStatus();

    auto link_types = _link_manager.extract_status();

    uint64_t tx_rate = 0;
    uint64_t rx_rate = 0;
    uint64_t peers = 0;
    for (const auto& links : link_types)
    {
      for (const auto& link : links)
      {
        if (link.empty())
          continue;
        for (const auto& peer : link["sessions"]["established"])
        {
          tx_rate += peer["tx"].get<uint64_t>();
          rx_rate += peer["rx"].get<uint64_t>();
          peers++;
        }
      }
    }

    // Compute all stats on all path builders on the default endpoint
    // Merge snodeSessions, remoteSessions and default into a single array
    std::vector<nlohmann::json> builders;

    if (services.is_object())
    {
      const auto& serviceDefault = services.at("default");
      builders.push_back(serviceDefault);

      auto snode_sessions = serviceDefault.at("snodeSessions");
      for (const auto& session : snode_sessions)
        builders.push_back(session);

      auto remote_sessions = serviceDefault.at("remoteSessions");
      for (const auto& session : remote_sessions)
        builders.push_back(session);
    }

    // Iterate over all items on this array to build the global pathStats
    uint64_t pathsCount = 0;
    uint64_t success = 0;
    uint64_t attempts = 0;
    for (const auto& builder : builders)
    {
      if (builder.is_null())
        continue;

      const auto& paths = builder.at("paths");
      if (paths.is_array())
      {
        for (const auto& [key, value] : paths.items())
        {
          if (value.is_object() && value.at("status").is_string()
              && value.at("status") == "established")
            pathsCount++;
        }
      }

      const auto& buildStats = builder.at("buildStats");
      if (buildStats.is_null())
        continue;

      success += buildStats.at("success").get<uint64_t>();
      attempts += buildStats.at("attempts").get<uint64_t>();
    }
    double ratio = static_cast<double>(success) / (attempts + 1);

    util::StatusObject stats{
        {"running", true},
        {"version", llarp::VERSION_FULL},
        {"uptime", to_json(Uptime())},
        {"numPathsBuilt", pathsCount},
        {"numPeersConnected", peers},
        {"numRoutersKnown", _node_db->num_loaded()},
        {"ratio", ratio},
        {"txRate", tx_rate},
        {"rxRate", rx_rate},
    };

    if (services.is_object())
    {
      stats["authCodes"] = services["default"]["authCodes"];
      stats["exitMap"] = services["default"]["exitMap"];
      stats["networkReady"] = services["default"]["networkReady"];
      stats["lokiAddress"] = services["default"]["identity"];
    }
    return stats;
  }

  void
  Router::Freeze()
  {
    if (IsServiceNode())
      return;

    for_each_connection(
        [this](link::Connection& conn) { loop()->call([&]() { conn.conn->close_connection(); }); });
  }

  void
  Router::Thaw()
  {
    if (IsServiceNode())
      return;

    std::unordered_set<RouterID> peer_pubkeys;

    for_each_connection(
        [&peer_pubkeys](link::Connection& conn) { peer_pubkeys.emplace(conn.remote_rc.pubkey); });

    loop()->call([this, &peer_pubkeys]() {
      for (auto& pk : peer_pubkeys)
        _link_manager.close_connection(pk);
    });
  }

  void
  Router::persist_connection_until(const RouterID& remote, llarp_time_t until)
  {
    _link_manager.set_conn_persist(remote, until);
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
    if (IsServiceNode())
    {
      return _rc_lookup_handler.get_random_whitelist_router(router);
    }

    if (auto maybe = node_db()->GetRandom([](const auto&) -> bool { return true; }))
    {
      router = maybe->pubkey;
      return true;
    }
    return false;
  }

  void
  Router::TriggerPump()
  {
    loop_wakeup->Trigger();
  }

  void
  Router::connect_to(const RouterID& rid)
  {
    _link_manager.connect_to(rid);
  }

  void
  Router::connect_to(const RouterContact& rc)
  {
    _link_manager.connect_to(rc);
  }

  void
  Router::lookup_router(RouterID rid, std::function<void(oxen::quic::message)> func)
  {
    _link_manager.send_control_message(
        rid,
        "find_router",
        FindRouterMessage::serialize(std::move(rid), false, false),
        std::move(func));
  }

  bool
  Router::send_data_message(const RouterID& remote, std::string payload)
  {
    return _link_manager.send_data_message(remote, std::move(payload));
  }

  bool
  Router::send_control_message(
      const RouterID& remote,
      std::string ep,
      std::string body,
      std::function<void(oxen::quic::message m)> func)
  {
    return _link_manager.send_control_message(
        remote, std::move(ep), std::move(body), std::move(func));
  }

  void
  Router::for_each_connection(std::function<void(link::Connection&)> func)
  {
    return _link_manager.for_each_connection(func);
  }

  bool
  Router::EnsureIdentity()
  {
    _encryption = _key_manager->encryptionKey;

    if (IsServiceNode())
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
      constexpr int maxTries = 5;
      int numTries = 0;
      while (numTries < maxTries)
      {
        numTries++;
        try
        {
          _identity = rpc_client()->ObtainIdentityKey();
          const RouterID pk{pubkey()};
          LogWarn("Obtained lokid identity key: ", pk);
          rpc_client()->StartPings();
          break;
        }
        catch (const std::exception& e)
        {
          LogWarn(
              "Failed attempt ",
              numTries,
              " of ",
              maxTries,
              " to get lokid identity keys because: ",
              e.what());

          if (numTries == maxTries)
            throw;
        }
      }
    }
    else
    {
      _identity = _key_manager->identityKey;
    }

    if (_identity.IsZero())
      return false;
    if (_encryption.IsZero())
      return false;

    return true;
  }

  bool
  Router::Configure(std::shared_ptr<Config> c, bool isSNode, std::shared_ptr<NodeDB> nodedb)
  {
    llarp::sys::service_manager->starting();

    _config = std::move(c);
    auto& conf = *_config;

    // Do logging config as early as possible to get the configured log level applied

    // Backwards compat: before 0.9.10 we used `type=file` with `file=|-|stdout` for print mode
    auto log_type = conf.logging.m_logType;
    if (log_type == log::Type::File
        && (conf.logging.m_logFile == "stdout" || conf.logging.m_logFile == "-"
            || conf.logging.m_logFile.empty()))
      log_type = log::Type::Print;

    if (log::get_level_default() != log::Level::off)
      log::reset_level(conf.logging.m_logLevel);
    log::clear_sinks();
    log::add_sink(log_type, log_type == log::Type::System ? "lokinet" : conf.logging.m_logFile);

    // re-add rpc log sink if rpc enabled, else free it
    if (_config->api.m_enableRPCServer and llarp::logRingBuffer)
      log::add_sink(llarp::logRingBuffer, llarp::log::DEFAULT_PATTERN_MONO);
    else
      llarp::logRingBuffer = nullptr;

    log::debug(logcat, "Configuring router");

    is_service_node = conf.router.m_isRelay;

    if (is_service_node)
    {
      rpc_addr = oxenmq::address(conf.lokid.lokidRPCAddr);
      _rpc_client = std::make_shared<rpc::LokidRpcClient>(_lmq, weak_from_this());
    }

    log::debug(logcat, "Starting RPC server");
    if (not StartRpcServer())
      throw std::runtime_error("Failed to start rpc server");

    if (conf.router.m_workerThreads > 0)
      _lmq->set_general_threads(conf.router.m_workerThreads);

    log::debug(logcat, "Starting OMQ server");
    _lmq->start();

    _node_db = std::move(nodedb);

    log::debug(
        logcat, is_service_node ? "Running as a relay (service node)" : "Running as a client");

    if (is_service_node)
    {
      _rpc_client->ConnectAsync(rpc_addr);
    }

    log::debug(logcat, "Initializing key manager");
    if (not _key_manager->initialize(conf, true, isSNode))
      throw std::runtime_error("KeyManager failed to initialize");

    log::debug(logcat, "Initializing from configuration");
    if (!from_config(conf))
      throw std::runtime_error("FromConfig() failed");

    log::debug(logcat, "Initializing identity");
    if (not EnsureIdentity())
      throw std::runtime_error("EnsureIdentity() failed");
    return true;
  }

  /// called in disk worker thread
  void
  Router::HandleSaveRC() const
  {
    std::string fname = our_rc_file.string();
    router_contact.Write(fname.c_str());
  }

  bool
  Router::SaveRC()
  {
    LogDebug("verify RC signature");
    if (!router_contact.Verify(now()))
    {
      Dump<MAX_RC_SIZE>(rc());
      LogError("RC is invalid, not saving");
      return false;
    }
    if (is_service_node)
      _node_db->put_rc(router_contact);
    queue_disk_io([&]() { HandleSaveRC(); });
    return true;
  }

  bool
  Router::IsServiceNode() const
  {
    return is_service_node;
  }

  bool
  Router::insufficient_peers() const
  {
    constexpr int KnownPeerWarningThreshold = 5;
    return node_db()->num_loaded() < KnownPeerWarningThreshold;
  }

  std::optional<std::string>
  Router::OxendErrorState() const
  {
    // If we're in the white or gray list then we *should* be establishing connections to other
    // routers, so if we have almost no peers then something is almost certainly wrong.
    if (appears_funded() and insufficient_peers())
      return "too few peer connections; lokinet is not adequately connected to the network";
    return std::nullopt;
  }

  void
  Router::Close()
  {
    log::info(logcat, "closing");
    if (_router_close_cb)
      _router_close_cb();
    log::debug(logcat, "stopping mainloop");
    _loop->stop();
    is_running.store(false);
  }

  bool
  Router::ParseRoutingMessageBuffer(
      const llarp_buffer_t&, path::AbstractHopHandler&, const PathID_t&)
  {
    // TODO: will go away with the removal of flush upstream/downstream
    return false;
  }

  bool
  Router::have_snode_whitelist() const
  {
    return IsServiceNode() and _rc_lookup_handler.has_received_whitelist();
  }

  bool
  Router::appears_decommed() const
  {
    return have_snode_whitelist() and _rc_lookup_handler.is_grey_listed(pubkey());
  }

  bool
  Router::appears_funded() const
  {
    return have_snode_whitelist() and _rc_lookup_handler.is_session_allowed(pubkey());
  }

  bool
  Router::appears_registered() const
  {
    return have_snode_whitelist() and _rc_lookup_handler.is_registered(pubkey());
  }

  bool
  Router::can_test_routers() const
  {
    return appears_funded();
  }

  bool
  Router::SessionToRouterAllowed(const RouterID& router) const
  {
    return _rc_lookup_handler.is_session_allowed(router);
  }

  bool
  Router::PathToRouterAllowed(const RouterID& router) const
  {
    if (appears_decommed())
    {
      // we are decom'd don't allow any paths outbound at all
      return false;
    }
    return _rc_lookup_handler.is_path_allowed(router);
  }

  size_t
  Router::NumberOfConnectedRouters() const
  {
    return _link_manager.get_num_connected();
  }

  size_t
  Router::NumberOfConnectedClients() const
  {
    return _link_manager.get_num_connected_clients();
  }

  bool
  Router::update_rc()
  {
    SecretKey nextOnionKey;
    RouterContact nextRC = router_contact;
    if (!nextRC.Sign(identity()))
      return false;
    if (!nextRC.Verify(time_now_ms(), false))
      return false;
    router_contact = std::move(nextRC);
    if (IsServiceNode())
      return SaveRC();
    return true;
  }

  bool
  Router::from_config(const Config& conf)
  {
    // Set netid before anything else
    log::debug(logcat, "Network ID set to {}", conf.router.m_netId);
    if (!conf.router.m_netId.empty()
        && strcmp(conf.router.m_netId.c_str(), llarp::DEFAULT_NETID) != 0)
    {
      const auto& netid = conf.router.m_netId;
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
      router_contact.netID = llarp::NetID();
    }

    // Router config
    _link_manager.max_connected_routers = conf.router.m_maxConnectedRouters;
    _link_manager.min_connected_routers = conf.router.m_minConnectedRouters;

    encryption_keyfile = _key_manager->m_encKeyPath;
    our_rc_file = _key_manager->m_rcPath;
    transport_keyfile = _key_manager->m_transportKeyPath;
    identity_keyfile = _key_manager->m_idKeyPath;

    if (auto maybe_ip = conf.links.PublicAddress)
      _ourAddress = var::visit([](auto&& ip) { return SockAddr{ip}; }, *maybe_ip);
    else if (auto maybe_ip = conf.router.PublicIP)
      _ourAddress = var::visit([](auto&& ip) { return SockAddr{ip}; }, *maybe_ip);

    if (_ourAddress)
    {
      if (auto maybe_port = conf.links.PublicPort)
        _ourAddress->setPort(*maybe_port);
      else if (auto maybe_port = conf.router.PublicPort)
        _ourAddress->setPort(*maybe_port);
      else
        throw std::runtime_error{"public ip provided without public port"};
      log::debug(logcat, "Using {} for our public address", *_ourAddress);
    }
    else
      log::debug(logcat, "No explicit public address given; will auto-detect during link setup");

    RouterContact::BlockBogons = conf.router.m_blockBogons;

    auto& networkConfig = conf.network;

    /// build a set of  strictConnectPubkeys (
    std::unordered_set<RouterID> strictConnectPubkeys;
    if (not networkConfig.m_strictConnect.empty())
    {
      const auto& val = networkConfig.m_strictConnect;
      if (IsServiceNode())
        throw std::runtime_error("cannot use strict-connect option as service node");
      if (val.size() < 2)
        throw std::runtime_error(
            "Must specify more than one strict-connect router if using strict-connect");
      strictConnectPubkeys.insert(val.begin(), val.end());
      log::debug(logcat, "{} strict-connect routers configured", val.size());
    }

    std::vector<fs::path> configRouters = conf.connect.routers;
    configRouters.insert(
        configRouters.end(), conf.bootstrap.files.begin(), conf.bootstrap.files.end());

    // if our conf had no bootstrap files specified, try the default location of
    // <DATA_DIR>/bootstrap.signed. If this isn't present, leave a useful error message
    // TODO: use constant
    fs::path defaultBootstrapFile = conf.router.m_dataDir / "bootstrap.signed";
    if (configRouters.empty() and conf.bootstrap.routers.empty())
    {
      if (fs::exists(defaultBootstrapFile))
      {
        configRouters.push_back(defaultBootstrapFile);
      }
    }

    bootstrap_rc_list.clear();
    for (const auto& router : configRouters)
    {
      log::debug(logcat, "Loading bootstrap router list from {}", defaultBootstrapFile);
      bootstrap_rc_list.AddFromFile(router);
    }

    for (const auto& rc : conf.bootstrap.routers)
    {
      bootstrap_rc_list.emplace(rc);
    }

    // in case someone has an old bootstrap file and is trying to use a bootstrap
    // that no longer exists
    auto clearBadRCs = [this]() {
      for (auto it = bootstrap_rc_list.begin(); it != bootstrap_rc_list.end();)
      {
        if (it->IsObsoleteBootstrap())
          log::warning(logcat, "ignoring obsolete boostrap RC: {}", RouterID{it->pubkey});
        else if (not it->Verify(now()))
          log::warning(logcat, "ignoring invalid bootstrap RC: {}", RouterID{it->pubkey});
        else
        {
          ++it;
          continue;
        }
        // we are in one of the above error cases that we warned about:
        it = bootstrap_rc_list.erase(it);
      }
    };

    clearBadRCs();

    if (bootstrap_rc_list.empty() and not conf.bootstrap.seednode)
    {
      auto fallbacks = llarp::load_bootstrap_fallbacks();
      if (auto itr = fallbacks.find(router_contact.netID.ToString()); itr != fallbacks.end())
      {
        bootstrap_rc_list = itr->second;
        log::debug(
            logcat, "loaded {} default fallback bootstrap routers", bootstrap_rc_list.size());
        clearBadRCs();
      }
      if (bootstrap_rc_list.empty() and not conf.bootstrap.seednode)
      {
        // empty after trying fallback, if set
        log::error(
            logcat,
            "No bootstrap routers were loaded.  The default bootstrap file {} does not exist, and "
            "loading fallback bootstrap RCs failed.",
            defaultBootstrapFile);
        throw std::runtime_error("No bootstrap nodes available.");
      }
    }

    if (conf.bootstrap.seednode)
      LogInfo("we are a seed node");
    else
      LogInfo("Loaded ", bootstrap_rc_list.size(), " bootstrap routers");

    // Init components after relevant config settings loaded
    _link_manager.init(&_rc_lookup_handler);
    _rc_lookup_handler.init(
        _contacts,
        _node_db,
        _loop,
        util::memFn(&Router::queue_work, this),
        &_link_manager,
        &_hidden_service_context,
        strictConnectPubkeys,
        bootstrap_rc_list,
        is_service_node);

    // FIXME: kludge for now, will be part of larger cleanup effort.
    if (is_service_node)
      InitInboundLinks();
    else
      InitOutboundLinks();

    // profiling
    _profile_file = conf.router.m_dataDir / "profiles.dat";

    // Network config
    if (conf.network.m_enableProfiling.value_or(false))
    {
      LogInfo("router profiling enabled");
      if (not fs::exists(_profile_file))
      {
        LogInfo("no profiles file at ", _profile_file, " skipping");
      }
      else
      {
        LogInfo("loading router profiles from ", _profile_file);
        router_profiling().Load(_profile_file);
      }
    }
    else
    {
      router_profiling().Disable();
      LogInfo("router profiling disabled");
    }

    // API config
    if (not IsServiceNode())
    {
      hidden_service_context().AddEndpoint(conf);
    }

    return true;
  }

  bool
  Router::CheckRenegotiateValid(RouterContact newrc, RouterContact oldrc)
  {
    return _rc_lookup_handler.check_renegotiate_valid(newrc, oldrc);
  }

  bool
  Router::IsBootstrapNode(const RouterID r) const
  {
    return std::count_if(
               bootstrap_rc_list.begin(),
               bootstrap_rc_list.end(),
               [r](const RouterContact& rc) -> bool { return rc.pubkey == r; })
        > 0;
  }

  bool
  Router::should_report_stats(llarp_time_t now) const
  {
    static constexpr auto ReportStatsInterval = 1h;
    return now - _last_stats_report > ReportStatsInterval;
  }

  void
  Router::report_stats()
  {
    const auto now = llarp::time_now_ms();
    LogInfo(node_db()->num_loaded(), " RCs loaded");
    LogInfo(bootstrap_rc_list.size(), " bootstrap peers");
    LogInfo(NumberOfConnectedRouters(), " router connections");
    if (IsServiceNode())
    {
      LogInfo(NumberOfConnectedClients(), " client connections");
      LogInfo(ToString(router_contact.Age(now)), " since we last updated our RC");
      LogInfo(ToString(router_contact.TimeUntilExpires(now)), " until our RC expires");
    }
    if (_last_stats_report > 0s)
      LogInfo(ToString(now - _last_stats_report), " last reported stats");
    _last_stats_report = now;
  }

  std::string
  Router::status_line()
  {
    std::string status;
    auto out = std::back_inserter(status);
    fmt::format_to(out, "v{}", fmt::join(llarp::VERSION, "."));
    if (IsServiceNode())
    {
      fmt::format_to(
          out,
          " snode | known/svc/clients: {}/{}/{}",
          node_db()->num_loaded(),
          NumberOfConnectedRouters(),
          NumberOfConnectedClients());
      fmt::format_to(
          out,
          " | {} active paths | block {} ",
          path_context().CurrentTransitPaths(),
          (_rpc_client ? _rpc_client->BlockHeight() : 0));
      auto maybe_last = _rcGossiper.LastGossipAt();
      fmt::format_to(
          out,
          " | gossip: (next/last) {} / {}",
          short_time_from_now(_rcGossiper.NextGossipAt()),
          maybe_last ? short_time_from_now(*maybe_last) : "never");
    }
    else
    {
      fmt::format_to(
          out,
          " client | known/connected: {}/{}",
          node_db()->num_loaded(),
          NumberOfConnectedRouters());

      if (auto ep = hidden_service_context().GetDefault())
      {
        fmt::format_to(
            out,
            " | paths/endpoints {}/{}",
            path_context().CurrentOwnedPaths(),
            ep->UniqueEndpoints());

        if (auto success_rate = ep->CurrentBuildStats().SuccessRatio(); success_rate < 0.5)
        {
          fmt::format_to(
              out, " [ !!! Low Build Success Rate ({:.1f}%) !!! ]", (100.0 * success_rate));
        }
      };
    }
    return status;
  }

  void
  Router::Tick()
  {
    if (is_stopping)
      return;
    // LogDebug("tick router");
    const auto now = llarp::time_now_ms();
    if (const auto delta = now - _last_tick; _last_tick != 0s and delta > TimeskipDetectedDuration)
    {
      // we detected a time skip into the futre, thaw the network
      LogWarn("Timeskip of ", ToString(delta), " detected. Resetting network state");
      Thaw();
    }

    llarp::sys::service_manager->report_periodic_stats();

    _pathbuild_limiter.Decay(now);

    router_profiling().Tick();

    if (should_report_stats(now))
    {
      report_stats();
    }

    _rcGossiper.Decay(now);

    _rc_lookup_handler.periodic_update(now);

    const bool has_whitelist = _rc_lookup_handler.has_received_whitelist();
    const bool is_snode = IsServiceNode();
    const bool is_decommed = appears_decommed();
    bool should_gossip = appears_funded();

    if (is_snode
        and (router_contact.ExpiresSoon(now, std::chrono::milliseconds(randint() % 10000)) or (now - router_contact.last_updated) > rc_regen_interval))
    {
      LogInfo("regenerating RC");
      if (update_rc())
      {
        // our rc changed so we should gossip it
        should_gossip = true;
        // remove our replay entry so it goes out
        _rcGossiper.Forget(pubkey());
      }
      else
        LogError("failed to update our RC");
    }
    if (should_gossip)
    {
      // if we have the whitelist enabled, we have fetched the list and we are in either
      // the white or grey list, we want to gossip our RC
      GossipRCIfNeeded(router_contact);
    }
    // remove RCs for nodes that are no longer allowed by network policy
    node_db()->RemoveIf([&](const RouterContact& rc) -> bool {
      // don't purge bootstrap nodes from nodedb
      if (IsBootstrapNode(rc.pubkey))
      {
        log::trace(logcat, "Not removing {}: is bootstrap node", rc.pubkey);
        return false;
      }
      // if for some reason we stored an RC that isn't a valid router
      // purge this entry
      if (not rc.IsPublicRouter())
      {
        log::debug(logcat, "Removing {}: not a valid router", rc.pubkey);
        return true;
      }
      /// clear out a fully expired RC
      if (rc.IsExpired(now))
      {
        log::debug(logcat, "Removing {}: RC is expired", rc.pubkey);
        return true;
      }
      // clients have no notion of a whilelist
      // we short circuit logic here so we dont remove
      // routers that are not whitelisted for first hops
      if (not is_snode)
      {
        log::trace(logcat, "Not removing {}: we are a client and it looks fine", rc.pubkey);
        return false;
      }

      // if we don't have the whitelist yet don't remove the entry
      if (not has_whitelist)
      {
        log::debug(logcat, "Skipping check on {}: don't have whitelist yet", rc.pubkey);
        return false;
      }
      // if we have no whitelist enabled or we have
      // the whitelist enabled and we got the whitelist
      // check against the whitelist and remove if it's not
      // in the whitelist OR if there is no whitelist don't remove
      if (has_whitelist and not _rc_lookup_handler.is_session_allowed(rc.pubkey))
      {
        log::debug(logcat, "Removing {}: not a valid router", rc.pubkey);
        return true;
      }
      return false;
    });

    if (not is_snode or not has_whitelist)
    {
      // find all deregistered relays
      std::unordered_set<PubKey> close_peers;

      for_each_connection([this, &close_peers](link::Connection& conn) {
        const auto& pk = conn.remote_rc.pubkey;

        if (conn.remote_is_relay and not _rc_lookup_handler.is_session_allowed(pk))
          close_peers.insert(pk);
      });

      // mark peers as de-registered
      for (auto& peer : close_peers)
        _link_manager.deregister_peer(std::move(peer));
    }

    _link_manager.check_persisting_conns(now);

    size_t connected = NumberOfConnectedRouters();

    const int interval = is_snode ? 5 : 2;
    const auto timepoint_now = std::chrono::steady_clock::now();
    if (timepoint_now >= _next_explore_at and not is_decommed)
    {
      _rc_lookup_handler.explore_network();
      _next_explore_at = timepoint_now + std::chrono::seconds(interval);
    }
    size_t connectToNum = _link_manager.min_connected_routers;
    const auto strictConnect = _rc_lookup_handler.num_strict_connect_routers();
    if (strictConnect > 0 && connectToNum > strictConnect)
    {
      connectToNum = strictConnect;
    }

    if (is_snode and now >= _next_decomm_warning)
    {
      constexpr auto DecommissionWarnInterval = 5min;
      if (auto registered = appears_registered(), funded = appears_funded();
          not(registered and funded and not is_decommed))
      {
        // complain about being deregistered/decommed/unfunded
        log::error(
            logcat,
            "We are running as a service node but we seem to be {}",
            not registered    ? "deregistered"
                : is_decommed ? "decommissioned"
                              : "not fully staked");
        _next_decomm_warning = now + DecommissionWarnInterval;
      }
      else if (insufficient_peers())
      {
        log::error(
            logcat,
            "We appear to be an active service node, but have only {} known peers.",
            node_db()->num_loaded());
        _next_decomm_warning = now + DecommissionWarnInterval;
      }
    }

    // if we need more sessions to routers and we are not a service node kicked from the network or
    // we are a client we shall connect out to others
    if (connected < connectToNum and (appears_funded() or not is_snode))
    {
      size_t dlt = connectToNum - connected;
      LogDebug("connecting to ", dlt, " random routers to keep alive");
      _link_manager.connect_to_random(dlt);
    }

    _hidden_service_context.Tick(now);
    _exit_context.Tick(now);

    // save profiles
    if (router_profiling().ShouldSave(now) and _config->network.m_saveProfiles)
    {
      queue_disk_io([&]() { router_profiling().Save(_profile_file); });
    }

    _node_db->Tick(now);

    std::set<dht::Key_t> peer_keys;

    for_each_connection(
        [&peer_keys](link::Connection& conn) { peer_keys.emplace(conn.remote_rc.pubkey); });

    _contacts->rc_nodes()->RemoveIf(
        [&peer_keys](const dht::Key_t& k) -> bool { return peer_keys.count(k) == 0; });

    paths.ExpirePaths(now);

    // update tick timestamp
    _last_tick = llarp::time_now_ms();
  }

  void
  Router::modify_rc(std::function<std::optional<RouterContact>(RouterContact)> modify)
  {
    if (auto maybe = modify(rc()))
    {
      router_contact = *maybe;
      update_rc();
      _rcGossiper.GossipRC(rc());
    }
  }

  bool
  Router::GetRandomConnectedRouter(RouterContact& result) const
  {
    return _link_manager.get_random_connected(result);
  }

  void
  Router::HandleDHTLookupForExplore(RouterID /*remote*/, const std::vector<RouterContact>& results)
  {
    for (const auto& rc : results)
    {
      _rc_lookup_handler.check_rc(rc);
    }
  }

  void
  Router::set_router_whitelist(
      const std::vector<RouterID>& whitelist,
      const std::vector<RouterID>& greylist,
      const std::vector<RouterID>& unfundedlist)
  {
    _rc_lookup_handler.set_router_whitelist(whitelist, greylist, unfundedlist);
  }

  bool
  Router::StartRpcServer()
  {
    if (_config->api.m_enableRPCServer)
      _rpc_server = std::make_unique<rpc::RPCServer>(_lmq, *this);

    return true;
  }

  bool
  Router::Run()
  {
    if (is_running || is_stopping)
      return false;

    // set public signing key
    router_contact.pubkey = seckey_topublic(identity());
    // set router version if service node
    if (IsServiceNode())
    {
      router_contact.routerVersion = RouterVersion(llarp::VERSION, llarp::constants::proto_version);
    }

    if (IsServiceNode() and not router_contact.IsPublicRouter())
    {
      LogError("we are configured as relay but have no reachable addresses");
      return false;
    }

    // set public encryption key
    router_contact.enckey = seckey_topublic(encryption());

    LogInfo("Signing rc...");
    if (!router_contact.Sign(identity()))
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
      _rcGossiper.Init(&_link_manager, us, this);
      // relays do not use profiling
      router_profiling().Disable();
    }
    else
    {
      // we are a client
      // regenerate keys and resign rc before everything else
      crypto::identity_keygen(_identity);
      crypto::encryption_keygen(_encryption);
      router_contact.pubkey = seckey_topublic(identity());
      router_contact.enckey = seckey_topublic(encryption());
      if (!router_contact.Sign(identity()))
      {
        LogError("failed to regenerate keys and sign RC");
        return false;
      }
    }

    LogInfo("starting hidden service context...");
    if (!hidden_service_context().StartAll())
    {
      LogError("Failed to start hidden service context");
      return false;
    }

    {
      LogInfo("Loading nodedb from disk...");
      _node_db->load_from_disk();
    }

    _contacts = std::make_shared<Contacts>(llarp::dht::Key_t(pubkey()), *this);

    for (const auto& rc : bootstrap_rc_list)
    {
      node_db()->put_rc(rc);
      _contacts->rc_nodes()->PutNode(rc);
      LogInfo("added bootstrap node ", RouterID{rc.pubkey});
    }

    LogInfo("have ", _node_db->num_loaded(), " routers");

    _loop->call_every(ROUTER_TICK_INTERVAL, weak_from_this(), [this] { Tick(); });
    _route_poker->start();
    is_running.store(true);
    _started_at = now();
    if (IsServiceNode())
    {
      // do service node testing if we are in service node whitelist mode
      _loop->call_every(consensus::REACHABILITY_TESTING_TIMER_INTERVAL, weak_from_this(), [this] {
        // dont run tests if we are not running or we are stopping
        if (not is_running)
          return;
        // dont run tests if we think we should not test other routers
        // this occurs when we are deregistered or do not have the service node list
        // yet when we expect to have one.
        if (not can_test_routers())
          return;
        auto tests = router_testing.get_failing();
        if (auto maybe = router_testing.next_random(this))
        {
          tests.emplace_back(*maybe, 0);
        }
        for (const auto& [router, fails] : tests)
        {
          if (not SessionToRouterAllowed(router))
          {
            LogDebug(
                router,
                " is no longer a registered service node so we remove it from the testing list");
            router_testing.remove_node_from_failing(router);
            continue;
          }
          LogDebug("Establishing session to ", router, " for SN testing");
          // try to make a session to this random router
          // this will do a dht lookup if needed
          _link_manager.connect_to(router);

          /*
           * TODO: container of pending snode test routers to be queried on
           *       connection success/failure, then do this stuff there.
          _outboundSessionMaker.CreateSessionTo(
              router, [previous_fails = fails, this](const auto& router, const auto result) {
                auto rpc = RpcClient();

                if (result != SessionResult::Establish)
                {
                  // failed connection mark it as so
                  m_routerTesting.add_failing_node(router, previous_fails);
                  LogInfo(
                      "FAILED SN connection test to ",
                      router,
                      " (",
                      previous_fails + 1,
                      " consecutive failures) result=",
                      result);
                }
                else
                {
                  m_routerTesting.remove_node_from_failing(router);
                  if (previous_fails > 0)
                  {
                    LogInfo(
                        "Successful SN connection test to ",
                        router,
                        " after ",
                        previous_fails,
                        " failures");
                  }
                  else
                  {
                    LogDebug("Successful SN connection test to ", router);
                  }
                }
                if (rpc)
                {
                  // inform as needed
                  rpc->InformConnection(router, result == SessionResult::Establish);
                }
              });
          */
        }
      });
    }
    llarp::sys::service_manager->ready();
    return is_running;
  }

  bool
  Router::IsRunning() const
  {
    return is_running;
  }

  llarp_time_t
  Router::Uptime() const
  {
    const llarp_time_t _now = now();
    if (_started_at > 0s && _now > _started_at)
      return _now - _started_at;
    return 0s;
  }

  void
  Router::AfterStopLinks()
  {
    llarp::sys::service_manager->stopping();
    Close();
    log::debug(logcat, "stopping oxenmq");
    _lmq.reset();
  }

  void
  Router::AfterStopIssued()
  {
    llarp::sys::service_manager->stopping();
    log::debug(logcat, "stopping links");
    StopLinks();
    log::debug(logcat, "saving nodedb to disk");
    node_db()->save_to_disk();
    _loop->call_later(200ms, [this] { AfterStopLinks(); });
  }

  void
  Router::StopLinks()
  {
    _link_manager.stop();
  }

  void
  Router::Die()
  {
    if (!is_running)
      return;
    if (is_stopping)
      return;

    is_stopping.store(true);
    if (log::get_level_default() != log::Level::off)
      log::reset_level(log::Level::info);
    LogWarn("stopping router hard");
    llarp::sys::service_manager->stopping();
    hidden_service_context().StopAll();
    _exit_context.Stop();
    StopLinks();
    Close();
  }

  void
  Router::Stop()
  {
    if (!is_running)
    {
      log::debug(logcat, "Stop called, but not running");
      return;
    }
    if (is_stopping)
    {
      log::debug(logcat, "Stop called, but already stopping");
      return;
    }

    is_stopping.store(true);
    if (auto level = log::get_level_default();
        level > log::Level::info and level != log::Level::off)
      log::reset_level(log::Level::info);
    log::info(logcat, "stopping");
    llarp::sys::service_manager->stopping();
    log::debug(logcat, "stopping hidden service context");
    hidden_service_context().StopAll();
    llarp::sys::service_manager->stopping();
    log::debug(logcat, "stopping exit context");
    _exit_context.Stop();
    llarp::sys::service_manager->stopping();
    log::debug(logcat, "final upstream pump");
    paths.PumpUpstream();
    llarp::sys::service_manager->stopping();
    log::debug(logcat, "final links pump");
    _loop->call_later(200ms, [this] { AfterStopIssued(); });
  }

  bool
  Router::HasSessionTo(const RouterID& remote) const
  {
    return _link_manager.have_connection_to(remote);
  }

  std::string
  Router::ShortName() const
  {
    return RouterID(pubkey()).ToString().substr(0, 8);
  }

  uint32_t
  Router::NextPathBuildNumber()
  {
    return _path_build_count++;
  }

  void
  Router::ConnectToRandomRouters(int _want)
  {
    const size_t want = _want;
    auto connected = NumberOfConnectedRouters();
    if (connected >= want)
      return;
    _link_manager.connect_to_random(want);
  }

  bool
  Router::InitServiceNode()
  {
    LogInfo("accepting transit traffic");
    paths.AllowTransit();
    _contacts->set_transit_allowed(true);
    _exit_context.AddExitEndpoint("default", _config->network, _config->dns);
    return true;
  }

  void
  Router::queue_work(std::function<void(void)> func)
  {
    _lmq->job(std::move(func));
  }

  void
  Router::queue_disk_io(std::function<void(void)> func)
  {
    _lmq->job(std::move(func), _disk_thread);
  }

  bool
  Router::HasClientExit() const
  {
    if (IsServiceNode())
      return false;
    const auto& ep = hidden_service_context().GetDefault();
    return ep and ep->HasExit();
  }

  oxen::quic::Address
  Router::public_ip() const
  {
    return _local_addr;
  }

  void
  Router::InitInboundLinks()
  {
    // auto addrs = _config->links.InboundListenAddrs;
    // if (is_service_node and addrs.empty())
    // {
    //   LogInfo("Inferring Public Address");

    //   auto maybe_port = _config->links.PublicPort;
    //   if (_config->router.PublicPort and not maybe_port)
    //     maybe_port = _config->router.PublicPort;
    //   if (not maybe_port)
    //     maybe_port = net::port_t::from_host(constants::DefaultInboundIWPPort);

    //   if (auto maybe_addr = net().MaybeInferPublicAddr(*maybe_port))
    //   {
    //     LogInfo("Public Address looks to be ", *maybe_addr);
    //     addrs.emplace_back(std::move(*maybe_addr));
    //   }
    // }
    // if (is_service_node and addrs.empty())
    //   throw std::runtime_error{"we are a service node and we have no inbound links configured"};

    // // create inbound links, if we are a service node
    // for (auto bind_addr : addrs)
    // {
    //   if (bind_addr.getPort() == 0)
    //     throw std::invalid_argument{"inbound link cannot use port 0"};

    //   if (net().IsWildcardAddress(bind_addr.getIP()))
    //   {
    //     if (auto maybe_ip = public_ip())
    //       bind_addr.setIP(public_ip().host());
    //     else
    //       throw std::runtime_error{"no public ip provided for inbound socket"};
    //   }

    //   AddressInfo ai;
    //   ai.fromSockAddr(bind_addr);

    //   _link_manager.connect_to({ai.IPString(), ai.port}, true);

    //   ai.pubkey = llarp::seckey_topublic(_identity);
    //   ai.dialect = "quicinet";  // FIXME: constant, also better name?
    //   ai.rank = 2;              // FIXME: hardcoded from the beginning...keep?
    //   AddAddressToRC(ai);
    // }
  }

  void
  Router::InitOutboundLinks()
  {
    // auto addrs = config()->links.OutboundLinks;
    // if (addrs.empty())
    //   addrs.emplace_back(net().Wildcard());

    // for (auto& bind_addr : addrs)
    // {
    //   _link_manager.connect_to({bind_addr.ToString()}, false);
    // }
  }

  const llarp::net::Platform&
  Router::net() const
  {
    return *llarp::net::Platform::Default_ptr();
  }

}  // namespace llarp
