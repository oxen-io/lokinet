#include "router.hpp"

#include <llarp/config/config.hpp>
#include <llarp/constants/proto.hpp>
#include <llarp/contact/contactdb.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/link/link_manager.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/util/formattable.hpp>
#include <llarp/util/logging.hpp>

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <utility>
#if defined(ANDROID) || defined(IOS)
#include <unistd.h>
#endif

#if defined(WITH_SYSTEMD)
#include <systemd/sd-daemon.h>
#endif

#include <llarp/constants/platform.hpp>

#include <oxenmq/oxenmq.h>

static constexpr std::chrono::milliseconds ROUTER_TICK_INTERVAL{250ms};

namespace llarp
{
    static auto logcat = log::Cat("router");

    Router::Router(
        Config conf, std::shared_ptr<quic::Loop> loop, std::shared_ptr<vpn::Platform> vpnPlatform, std::promise<void> p)
        : _config{std::move(conf)},
          _loop{std::move(loop)},
          _next_explore_at{std::chrono::steady_clock::now()},
          _omq{std::make_unique<oxenmq::OxenMQ>()},
          _close_promise{std::move(p)},
          _vpn{std::move(vpnPlatform)},
          _contact_db{std::make_unique<ContactDB>(*this)},
          _node_db{std::make_unique<NodeDB>(nodedb_dirname, *this)},
          _disk_thread{_omq->add_tagged_thread("disk")},
          // TODO FIXME: what about non-testnet?  And do we really want a fixed random interval,
          // or do we want a randomized interval on each node's gossip?
          _gossip_interval{TESTNET_GOSSIP_INTERVAL(llarp::csrng)},
          _last_tick{llarp::time_now_ms()}
    {
        // for oxend, so we don't close the connection when syncing the whitelist (which exceeds the
        // defaut 1MB limit).
        _omq->MAX_MSG_SIZE = -1;
        if (_config.router.worker_threads > 0)
            _omq->set_general_threads(_config.router.worker_threads);

        init_logging();

        configure();
        start();
    }

    // Default, but we define it here because some of the unique_ptrs are for forward-declared types
    // in router.hpp which aren't available for destruction, but are available here.
    Router::~Router() = default;

    nlohmann::json Router::ExtractStatus() const
    {
        if (not _is_running)
            nlohmann::json{{"running", false}};

        auto [in, out, relay, client] = _link_manager->connection_stats();
        auto [rcs, rids, bootstraps] = node_db().db_stats();
        auto [npaths, nhops] = path_context.path_ctx_stats();
        auto [nsessions, is_exit] = _session_endpoint->session_stats();
        auto ccs = contact_db().num_ccs();

        return {
            {"instance",
             {{"id", local_rid().to_network_address(_is_service_node)}, {"running", true}, {"exit_node", is_exit}}},
            {"links", {{"inbound", in}, {"outbound", out}, {"relay", relay}, {"client", client}}},
            {"sessions", {{"active", nsessions}}},
            {"nodedb", {{"RCs", rcs}, {"RIDs", rids}}},
            {"contactdb", {{"CCs", ccs}}},
            {"path_ctx", {{"paths", npaths}, {"hops", nhops}}}};
    }

    nlohmann::json Router::ExtractSummaryStatus() const
    {
        // if (!is_running)
        //   return nlohmann::json{{"running", false}};

        // auto services = _hidden_service_context.ExtractStatus();

        // auto link_types = _link_manager->extract_status();

        // uint64_t tx_rate = 0;
        // uint64_t rx_rate = 0;
        // uint64_t peers = 0;
        // for (const auto& links : link_types)
        // {
        //   for (const auto& link : links)
        //   {
        //     if (link.empty())
        //       continue;
        //     for (const auto& peer : link["sessions"]["established"])
        //     {
        //       tx_rate += peer["tx"].get<uint64_t>();
        //       rx_rate += peer["rx"].get<uint64_t>();
        //       peers++;
        //     }
        //   }
        // }

        // // Compute all stats on all path builders on the default endpoint
        // // Merge snodeSessions, remoteSessions and default into a single array
        // std::vector<nlohmann::json> builders;

        // if (services.is_object())
        // {
        //   const auto& serviceDefault = services.at("default");
        //   builders.push_back(serviceDefault);

        //   auto snode_sessions = serviceDefault.at("snodeSessions");
        //   for (const auto& session : snode_sessions)
        //     builders.push_back(session);

        //   auto remote_sessions = serviceDefault.at("remoteSessions");
        //   for (const auto& session : remote_sessions)
        //     builders.push_back(session);
        // }

        // // Iterate over all items on this array to build the global pathStats
        // uint64_t pathsCount = 0;
        // uint64_t success = 0;
        // uint64_t attempts = 0;
        // for (const auto& builder : builders)
        // {
        //   if (builder.is_null())
        //     continue;

        //   const auto& paths = builder.at("paths");
        //   if (paths.is_array())
        //   {
        //     for (const auto& [key, value] : paths.items())
        //     {
        //       if (value.is_object() && value.at("status").is_string()
        //           && value.at("status") == "established")
        //         pathsCount++;
        //     }
        //   }

        //   const auto& buildStats = builder.at("buildStats");
        //   if (buildStats.is_null())
        //     continue;

        //   success += buildStats.at("success").get<uint64_t>();
        //   attempts += buildStats.at("attempts").get<uint64_t>();
        // }
        // double ratio = static_cast<double>(success) / (attempts + 1);

        nlohmann::json stats{
            {"running", true},
            {"version", llarp::LOKINET_VERSION_FULL},
            {"uptime", to_json(Uptime())},
            // {"numPathsBuilt", pathsCount},
            // {"numPeersConnected", peers},
            {"numRoutersKnown", _node_db->num_rcs()},
            // {"ratio", ratio},
            // {"txRate", tx_rate},
            // {"rxRate", rx_rate},
        };

        // if (services.is_object())
        // {
        //   stats["authCodes"] = services["default"]["authCodes"];
        //   stats["exitMap"] = services["default"]["exitMap"];
        //   stats["networkReady"] = services["default"]["networkReady"];
        //   stats["lokiAddress"] = services["default"]["identity"];
        // }
        return stats;
    }

    void Router::queue_work(std::function<void()> func) { _omq->job(std::move(func)); }
    void Router::queue_disk_io(std::function<void()> func) { _omq->job(std::move(func), _disk_thread); }

    void Router::start_tickers()
    {
        if (_tun)
            _tun->start_poller();

        if (not _systemd_ticker->is_running())
        {
            if (not _systemd_ticker->start())
                throw std::runtime_error{"Failed to start system service report ticker!"};

            log::debug(logcat, "Successfully started system service report ticker");
        }
        else
            log::debug(logcat, "System service report ticker already auto-started!");

        _node_db->start_tickers();
        _contact_db->start_tickers();

        if (is_service_node())
        {
            _rpc_client->start_pings();
            _link_manager->start_tickers();

            if (not _testing_disabled)
            {
                if (not _reachability_ticker)
                    throw std::runtime_error{"Router has no service node reachability loop ticker -- Does not exist!"};

                if (not _reachability_ticker->is_running())
                {
                    if (not _reachability_ticker->start())
                        throw std::runtime_error{"Router failed to start service node reachability loop ticker!"};

                    log::debug(logcat, "Router successfully started service node reachability loop ticker!");
                }
                else
                    log::debug(logcat, "Service node reachability loop ticker already auto-started!");
            }
        }
        else
        {
            _session_endpoint->start_tickers();
            // Resolve needed ONS values now that we have the necessary things prefigured
            _session_endpoint->resolve_sns_mappings();
        }
    }

    bool Router::is_fully_meshed() const { return num_router_connections() >= _node_db->num_rcs(); }

    void Router::persist_connection_until(const RouterID& remote, std::chrono::milliseconds until)
    {
        _link_manager->set_conn_persist(remote, until);
    }

    bool Router::send_data_message(const RouterID& remote, std::string payload)
    {
        return _link_manager->send_data_message(remote, std::move(payload));
    }

    bool Router::send_control_message(
        const RouterID& remote, std::string ep, std::string body, std::function<void(quic::message)> func)
    {
        return _link_manager->send_control_message(remote, std::move(ep), std::move(body), std::move(func));
    }

    std::unordered_set<RouterID> Router::get_current_remotes() const { return _link_manager->get_current_remotes(); }

    void Router::for_each_connection(std::function<void(const RouterID&, link::Connection&)> func)
    {
        return _link_manager->for_each_connection(std::move(func));
    }

    void Router::fetch_snode_identity()
    {
        assert(_is_service_node);

        our_rc_file = _config.router.data_dir / our_rc_filename;

#if defined(ANDROID) || defined(IOS)
        log::critical(logcat, "running a service node on mobile devices is not possible.");
        throw std::runtime_error{"Invalid SNode configuration"};
#elif defined(_WIN32)
        log::critical(logcat, "running a service node on windows is not possible.");
        throw std::runtime_error{"Invalid SNode configuration"};
#endif
        constexpr int maxTries = 5;
        int numTries = 0;
        while (numTries < maxTries)
        {
            numTries++;
            try
            {
                key_manager.update_idkey(rpc_client()->obtain_identity_key());
                log::warning(logcat, "Obtained oxend identity key: {}", key_manager.router_id());
                break;
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Failed attempt {} of {} to get oxend id keys: ", numTries, maxTries, e.what());

                if (numTries == maxTries)
                    throw;
            }
        }
    }

    void Router::init_logging()
    {
        if (_config.logging.type)
        {
            auto log_type = *_config.logging.type;

            // Backwards compat: before 0.9.10 we used `type=file` with `file=|-|stdout` for print mode
            if (log_type == log::Type::File
                && (_config.logging.file == "stdout" || _config.logging.file == "-" || _config.logging.file.empty()))
                log_type = log::Type::Print;

            log::clear_sinks();
            log::add_sink(log_type, log_type == log::Type::System ? "lokinet" : _config.logging.file);
        }

        log::apply_categories(_config.logging.levels);

        // re-add rpc log sink if rpc enabled, else free it
        if (_config.api.enable_rpc_server and llarp::logRingBuffer)
            log::add_sink(llarp::logRingBuffer, llarp::log::DEFAULT_PATTERN_MONO);
        else
            llarp::logRingBuffer.reset();
    }

    void Router::init_bootstrap()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (_bootstrap_seed = _config.bootstrap.seednode; _bootstrap_seed)
            log::critical(logcat, "Local instance is bootstrap seed node!");

        node_db().populate_bootstraps(
            _config.bootstrap.files, _config.router.data_dir / "bootstrap.signed", not _bootstrap_seed);
    }

    void Router::process_routerconfig()
    {
        // Router config
        min_client_outbounds = _config.router.client_router_connections;

        auto paddr = _config.router.public_ip;
        if (!paddr)
            paddr = _config.links.public_addr;
        auto pport = _config.router.public_port;
        if (!pport)
            pport = _config.links.public_port;

        if (pport and not paddr)
            throw std::runtime_error{"If public-port is specified, public-addr must be as well!"};

        if (_config.links.listen_addr or not _is_service_node)
        {
            _listen_address = _config.links.listen_addr.value_or(DEFAULT_CLIENT_LISTEN_ADDR);

            log::info(
                logcat,
                "Using {} listen address: {}",
                _config.links.listen_addr ? "link config" : "default",
                _listen_address);
        }
        else
        {
            if (paddr or pport)
                throw std::runtime_error{"Must specify [bind]:listen in config with public ip/addr!"};

            log::critical(logcat, "No value in link config listen_addr, querying net-if...");
            if (auto maybe_addr = net().get_best_public_address(true, DEFAULT_LISTEN_PORT))
                _listen_address = std::move(*maybe_addr);
            else
                throw std::runtime_error{"Could not find net interface on current platform!"};
        }

        if (_is_service_node)
        {
            _public_address = (not paddr and not pport) ? _listen_address
                                                        : quic::Address{*paddr, pport.value_or(DEFAULT_LISTEN_PORT)};
        }
        else if (_listen_address.is_addressable())
        {
            log::info(logcat, "Assigning addressable listen address {} as public addr", _listen_address);
            _public_address = _listen_address;
        }
        else if (_is_service_node)  // TODO: check if this if is correct
        {
            log::critical(logcat, "Listen address is non-public, querying net-if for public address...");
            auto _port = !_listen_address.is_any_port() and _config.links.only_user_port ? _listen_address.port()
                                                                                         : DEFAULT_LISTEN_PORT;
            if (auto maybe_addr = net().get_best_public_address(true, _port))
                _public_address = std::move(*maybe_addr);
            else
            {
                log::critical(logcat, "Could not find net interface on current platform!");
                throw std::runtime_error{
                    "Unable to determine public IP; you must set public-port and public-addr config settings"};
            }
        }

        RelayContact::BLOCK_BOGONS = _config.router.block_bogons;
    }

    void Router::process_netconfig()
    {
        auto& conf = _config.network;

        if (!conf._if_name)
            conf._if_name = net().find_free_tun();

        if (!(conf._local_ip_net && conf._local_ip_net->ip.addr))
        {
            if (auto maybe = net().find_free_ipv4_net(conf._local_ip_net ? conf._local_ip_net->mask : 16))
                conf._local_ip_net = std::move(*maybe);
            else
                throw std::runtime_error("cannot find free IPv4 address range!");
        }
        log::info(logcat, "Lokinet {} IPv4 local network is {}", *conf._if_name, conf._local_ip_net);

        if (conf.enable_ipv6)
        {
            if (!conf._local_ipv6_net || (!conf._local_ipv6_net->ip.hi && !conf._local_ipv6_net->ip.lo))
            {
                if (auto maybe = net().find_free_ipv6_net(conf._local_ipv6_net ? conf._local_ipv6_net->mask : 64))
                    conf._local_ipv6_net = std::move(*maybe);
                else
                    throw std::runtime_error("cannot find free IPv6 address range!");
            }
            log::info(logcat, "Lokinet {} IPv6 local network is {}", *conf._if_name, *conf._local_ipv6_net);
            log::warning(
                logcat, "Lokinet IPv6 support is a work-in-progress and unsupported; enabling it is not recommended");
        }

        // Make sure any reserved addresses are within our local network range:
        std::erase_if(conf._reserved_local_ipv4, [&conf](const auto& addr_ip) {
            return !conf._local_ip_net->contains(addr_ip.second);
        });
        if (conf._local_ipv6_net)
            std::erase_if(conf._reserved_local_ipv6, [&conf](const auto& addr_ip) {
                return !conf._local_ipv6_net->contains(addr_ip.second);
            });

        // parse strict-connet pubkeys
        if (auto& conf_edges = conf.pinned_edges; not conf_edges.empty())
        {
            if (is_service_node())
                throw std::runtime_error("cannot use strict-connect option as service node");

            auto n_edges = static_cast<int>(conf_edges.size());

            // bad inputs throw in config parsing, so we should never have 0 pinned_edges
            assert(n_edges > 0);

            if (not n_edges)
                throw std::runtime_error(
                    "Must specify at least ONE valid strict-connect relay if using [network]:strict-connect");

            node_db().set_pinned_edges(std::move(conf_edges));

            // TODO: load strict-connects as bootstraps as well

            log::debug(logcat, "Local client configured to strictly use {} edge relays", n_edges);

            if (min_client_outbounds > n_edges)
            {
                min_client_outbounds = n_edges;
                log::debug(
                    logcat,
                    "Local client holds only {} strict-connect edge relays; adjusting minimum router connections "
                    "commensurately",
                    n_edges);
            }
        }
        else
            log::debug(
                logcat, "Local client configured to maintain {} router connections at minimum", min_client_outbounds);

        if (not min_client_outbounds)
            throw std::runtime_error{"Client must be configured to have at least 1 outbound router connection!"};
    }

    void Router::init_tun()
    {
        if (_tun = _loop->template make_shared<handlers::TunEndpoint>(*this); _tun != nullptr)
            _tun->configure();
        else
            throw std::runtime_error{"Failed to construct TunEndpoint API!"};
    }

    void Router::configure()
    {
        _loop->call_get([this] {
            llarp::sys::service_manager->starting();

            if (_is_exit_node and _is_service_node)
                throw std::runtime_error{
                    "Lokinet cannot simultaneously operate as a service node and client-operated exit node service!"};

            if (_config.lokid.disable_testing && netid() == NetID::MAINNET)
                throw std::runtime_error{"Error: reachability testing can only be disabled on testnet!"};

            auto net_id = netid();
            log::log(
                logcat, net_id == NetID::MAINNET ? log::Level::debug : log::Level::warn, "Network ID is {}", net_id);

            log::trace(logcat, "Configuring router...");

            _omq->log_level(oxenlog_to_omq_level(log::get_level_default()));

            log::info(
                logcat,
                "Local instance operating in {} mode{}",
                _is_service_node ? "relay" : "client",
                _is_exit_node ? " operating an exit node service!" : "!");

            if (_is_service_node)
            {
                log::debug(logcat, "Starting RPC client");
                rpc_addr = oxenmq::address(_config.lokid.rpc_addr);
                _rpc_client = std::make_unique<rpc::RPCClient>(*_omq, *this);
            }

            if (_config.api.enable_rpc_server)
            {
                log::debug(logcat, "Starting RPC server");
                _rpc_server = std::make_unique<rpc::RPCServer>(*_omq, *this);
            }

            log::debug(logcat, "Starting OMQ server");
            _omq->start();

            if (_is_service_node)
            {
                log::trace(logcat, "RPC client connecting to RPC bind address");
                _rpc_client->connect_async(rpc_addr);
            }

            log::debug(logcat, "Initializing key manager");

            if (_is_service_node)
                fetch_snode_identity();
            else
                key_manager = KeyManager{_config, _is_service_node};

            log::trace(logcat, "Initializing from configuration");

            process_routerconfig();

            log::debug(
                logcat,
                "public addr={}, listen addr={}",
                _public_address ? _public_address->to_string() : "< NONE >",
                _listen_address);

            // We process the relevant netconfig values (ip_range, address, and ip) here; in case the range or interface
            // is bad, we search for a free one and set it BACK into the config. Every subsequent object configuring
            // using the NetworkConfig (ex: tun/null, exit::Handler, etc) will have processed values
            process_netconfig();

            init_bootstrap();
            _node_db->configure();

            relay_contact = {identity(), _is_service_node and _public_address ? *_public_address : _listen_address};

            if (not relay_contact.is_public_addressable())
            {
                constexpr auto err = "Router is configured as relay but has no reachable address!";
                log::critical(logcat, "{}", err);
                throw std::runtime_error{err};
            }

            _session_endpoint = std::make_unique<handlers::SessionEndpoint>(*this);

            log::debug(logcat, "Creating QUIC link manager...");
            _link_manager = std::make_unique<LinkManager>(*this);

            // API config
            //  Full clients have TUN
            //  Embedded clients have nothing
            //  All relays have TUN
            if (_config.network.init_tun)
            {
                log::debug(logcat, "Initializing virtual TUN device...");
                _tun = _loop->make_shared<handlers::TunEndpoint>(*this);
            }
            else
                log::debug(logcat, "Not initializing TUN device; disabled in config.");
        });
    }

    bool Router::is_service_node() const { return _is_service_node; }

    bool Router::is_exit_node() const { return _is_exit_node; }

    bool Router::insufficient_peers() const
    {
        constexpr int KnownPeerWarningThreshold = 5;
        return node_db().num_rcs() < KnownPeerWarningThreshold;
    }

    std::optional<std::string> Router::OxendErrorState() const
    {
        // If we're in the white or gray list then we *should* be establishing connections to other
        // routers, so if we have almost no peers then something is almost certainly wrong.
        if (appears_funded() and insufficient_peers() and not _testing_disabled)
            return "too few peer connections; lokinet is not adequately connected to the network";
        return std::nullopt;
    }

    bool Router::has_whitelist() const { return whitelist_received; }

    bool Router::appears_decommed() const
    {
        return _is_service_node and has_whitelist() and not node_db().registered_routers().count(local_rid());
    }

    bool Router::appears_funded() const
    {
        return _is_service_node and has_whitelist() and node_db().is_connection_allowed(local_rid());
    }

    bool Router::appears_registered() const
    {
        return _is_service_node and has_whitelist() and node_db().registered_routers().count(local_rid());
    }

    bool Router::can_test_routers() const { return appears_funded() and not _testing_disabled; }

    size_t Router::num_router_connections(bool active_only) const
    {
        return _link_manager->get_num_connected_routers(active_only);
    }

    size_t Router::num_client_connections() const { return _link_manager->get_num_connected_clients(); }

    RemoteRC Router::update_rc_for_gossiping()
    {
        relay_contact.resign();
        save_rc();
        return relay_contact.to_remote();
    }

    void Router::save_rc()
    {
        queue_disk_io([&]() {
            log::info(logcat, "Saving RC file to {}", our_rc_file);
            relay_contact.write(our_rc_file);
        });
    }

    bool Router::should_report_stats(std::chrono::milliseconds now) const
    {
        return now - _last_stats_report > REPORT_STATS_INTERVAL;
    }

    std::string Router::_stats_line()
    {
        auto [_in, _out, _relay, _client] = _link_manager->connection_stats();
        auto [_rcs, _rids, _] = _node_db->db_stats();
        auto [_npaths, _nhops] = path_context.path_ctx_stats();
        auto _nsessions = std::get<0>(_session_endpoint->session_stats());
        auto _ccs = _contact_db->num_ccs();

        return "{}RCs:{} | RIDs:{} | CCs:{} | {}:{} | sessions:{} | conns:[ in:{} | out:{} | relay:{} | client:{} ]"_format(
            _is_service_node ? "Full Mesh:{} | "_format(detail::bool_alpha(_relay == _rcs, "YES", "NO")) : "",
            _rcs,
            _rids,
            _ccs,
            _is_service_node ? "hops" : "paths",
            _is_service_node ? _nhops : _npaths,
            _nsessions,
            _in,
            _out,
            _relay,
            _client);
    }

    void Router::report_stats()
    {
        const auto now = llarp::time_now_ms();

        log::critical(logcat, "Local {}: [ {} ]", is_service_node() ? "Service Node" : "Client", _stats_line());

        if (_last_stats_report > 0s)
            log::trace(logcat, "Last reported stats time {}", now - _last_stats_report);

        _last_stats_report = now;

        oxen::log::flush();
    }

    std::string Router::status_line()
    {
        auto line = "v{}{}: {}"_format(
            fmt::join(llarp::LOKINET_VERSION, "."), (_is_service_node) ? " snode: " : " client: ", _stats_line());

        if (is_service_node())
            line += ", gossip interval={}"_format(_gossip_interval);

        return line;
    }

    void Router::_relay_tick(std::chrono::milliseconds now)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        const auto& local = local_rid();

        // TESTNET:
        if (not node_db().registered_routers().count(local))
        {
            log::trace(logcat, "We are NOT a registered router, figure it out!");
            return;
        }

        sys::service_manager->report_periodic_stats();

        if (should_report_stats(now))
            report_stats();

        if (not _node_db->tick(now))
        {
            log::trace(logcat, "Router awaiting NodeDB completion to proceed with ::tick() logic...");
            return;
        }

        _link_manager->check_persisting_conns(now);

        const bool is_decommed = appears_decommed();
        // we want ALL router-connections, including in-progress connections because full-mesh
        auto num_router_conns = num_router_connections(false);

        if (now >= _next_decomm_warning)
        {
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
                _next_decomm_warning = now + DECOMM_WARNING_INTERVAL;
            }
            else if (insufficient_peers())
            {
                log::error(
                    logcat,
                    "We appear to be an active service node, but have only {} known peers.",
                    node_db().num_rcs());
                _next_decomm_warning = now + DECOMM_WARNING_INTERVAL;
            }
        }

        if (num_router_conns < node_db().num_rcs())
        {
            log::debug(
                logcat, "Service Node connecting to {} random routers to achieve full mesh", FULL_MESH_ITERATION);
            _link_manager->connect_to_keep_alive(FULL_MESH_ITERATION);
        }

        path_context.expire_hops(now);
    }

    void Router::_client_tick(std::chrono::milliseconds now)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        llarp::sys::service_manager->report_periodic_stats();
        _pathbuild_limiter.Decay(now);
        _router_profiling.tick();

        if (now > _last_path_ping + 1s)
        {
            _last_path_ping = now;
            _session_endpoint->ping_paths(now);
        }

        if (should_report_stats(now))
            report_stats();

        if (not _node_db->tick(now))
        {
            log::trace(logcat, "Router awaiting NodeDB completion to proceed with ::tick() logic...");
            return;
        }

        // TODO: make "use_pinned_edges" boolean to only connect to pinned edges
        // if we need more sessions to routers we shall connect out to others
        if (auto n_conns = static_cast<int>(num_router_connections()); n_conns < min_client_outbounds)
        {
            // result could maybe be negative with this subtraction, so we HAVE to check nconns < min in the conditional
            auto num_needed = min_client_outbounds - n_conns;

            log::critical(
                logcat,
                "Client connecting to {} random routers to keep alive (current:{}, needed:{})",
                num_needed,
                n_conns,
                min_client_outbounds);
            _link_manager->connect_to_keep_alive(num_needed);

            if (num_needed == min_client_outbounds - 1)  // subtract bootstrap
            {
                log::info(
                    logcat,
                    "Client has 0 non-bootstrap router connections currently; bypassing SessionEndpoint tick...");
                return;
            }
        }
        else
            initial_client_connect_complete = true;

        if (initial_client_connect_complete)
            _session_endpoint->tick(now);
    }

    void Router::tick()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (_is_stopping)
        {
            log::critical(logcat, "Router is stopping; exiting ::tick()...");
            return;
        }

        const auto now = llarp::time_now_ms();

        if (const auto delta = now - _last_tick; _last_tick != 0s and delta > NETWORK_RESET_SKIP_INTERVAL)
        {
            // TODO: this, if needed?
            // we detected a time skip into the futre, thaw the network
            log::error(logcat, "Timeskip of {} detected, resetting network state!", delta.count());
        }

        _is_service_node ? _relay_tick(now) : _client_tick(now);

        // update tick timestamp
        _last_tick = llarp::time_now_ms();
    }

    const std::unordered_set<RouterID>& Router::get_whitelist() const { return _node_db->registered_routers(); }

    void Router::set_router_whitelist(const std::vector<RouterID>& whitelist)
    {
        node_db().set_router_whitelist(whitelist);
        whitelist_received = true;
    }

    void Router::start()
    {
        log::debug(logcat, "{} called", __PRETTY_FUNCTION__);

        if (is_service_node())
        {
            save_rc();

            log::info(logcat, "Router accepting transit traffic...");
            path_context.allow_transit();

            // relays do not use profiling
            _router_profiling.disable();

            log::info(logcat, "Router initialized as service node!");
        }
        else if (netid() == NetID::MAINNET and _config.network.enable_profiling)
        {
            _router_profiling._profile_file = _config.router.data_dir / "profiles.dat";

            log::debug(logcat, "Router profiling enabled");
            if (not fs::exists(_router_profiling._profile_file))
            {
                log::debug(logcat, "No profiles file found at {}; skipping...", _router_profiling._profile_file);
            }
            else
            {
                log::debug(logcat, "Loading router profiles from {}", _router_profiling._profile_file);
                _router_profiling.load_from_disk();
            }

            if (_config.network.save_profiles)
            {
                log::debug(logcat, "Router profile saving enabled");
                _router_profiling.start_save_ticker(*this);
            }
        }
        else
        {
            _config.network.enable_profiling = false;
            _router_profiling.disable();
            log::info(logcat, "Router profiling disabled");
        }

        log::debug(logcat, "Creating Router::Tick() repeating event...");
        _loop_ticker = _loop->call_every(ROUTER_TICK_INTERVAL, [this] { tick(); }, false);

        _systemd_ticker = _loop->call_every(
            SERVICE_MANAGER_REPORT_INTERVAL, []() { sys::service_manager->report_periodic_stats(); }, false, true);

        _started_at = now();

        if (is_service_node() and not _testing_disabled)
        {
            log::debug(logcat, "Creating reachability testing ticker...");
            // do service node testing if we are in service node whitelist mode
            _reachability_ticker = _loop->call_every(
                consensus::REACHABILITY_TESTING_TIMER_INTERVAL,
                [this] {
                    // dont run tests if we are not running or we are stopping
                    if (not _is_running)
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
                        if (not _node_db->is_connection_allowed(router))
                        {
                            log::debug(
                                logcat,
                                "{} is no longer a registered service node; dropping from test "
                                "list",
                                router);
                            router_testing.remove_node_from_failing(router);
                            continue;
                        }

                        log::critical(logcat, "Establishing session to {} for service node testing", router);

                        // try to make a session to this random router
                        // this will do a dht lookup if needed
                        _link_manager->test_reachability(
                            router,
                            [this, rid = router, previous = fails](quic::connection_interface& conn) {
                                log::info(
                                    logcat,
                                    "Successful SN reachability test to {}{}",
                                    rid,
                                    previous ? "after {} previous failures"_format(previous) : "");
                                router_testing.remove_node_from_failing(rid);
                                _rpc_client->inform_connection(rid, true);
                                conn.close_connection();
                            },
                            [this, rid = router, previous = fails](quic::connection_interface&, uint64_t ec) {
                                if (ec != 0)
                                {
                                    log::info(
                                        logcat,
                                        "Unsuccessful SN reachability test to {} after {} previous "
                                        "failures",
                                        rid,
                                        previous);
                                    router_testing.add_failing_node(rid, previous);
                                }
                            });
                    }
                },
                false);
        }

        log::critical(logcat, "\n\n\tLOCAL INSTANCE ROUTER ID: {}\n", local_rid().to_network_address(_is_service_node));

        start_tickers();
        _is_running = true;

        llarp::sys::service_manager->ready();
    }

    std::chrono::milliseconds Router::Uptime() const
    {
        const std::chrono::milliseconds _now = now();
        if (_started_at > 0s && _now > _started_at)
            return _now - _started_at;
        return 0s;
    }

    void Router::close()
    {
        log::debug(logcat, "closing");

        if (_router_close_cb)
            _router_close_cb();

        _is_running.store(false);
    }

    void Router::teardown()
    {
        close();
        log::debug(logcat, "stopping oxenmq");
        _omq.reset();
        _close_promise.set_value();
    }

    void Router::cleanup()
    {
        log::debug(logcat, "stopping outbound links");
        stop_outbounds();

        log::debug(logcat, "cleaning up nodedb");
        node_db().save_to_disk();

        log::debug(logcat, "cleaning up link_manager");
        _link_manager.reset();

        _loop->call_later(200ms, [this] { teardown(); });
    }

    void Router::stop_outbounds()
    {
        _link_manager->close_all_links();

        auto rv = _loop_ticker->stop();
        log::debug(logcat, "router loop ticker stopped {}successfully!", rv ? "" : "un");
        _loop_ticker.reset();

        rv = _systemd_ticker->stop();
        log::debug(logcat, "systemd ticker stopped {}successfully!", rv ? "" : "un");
        _systemd_ticker.reset();

        if (_reachability_ticker)
        {
            log::debug(logcat, "clearing reachability ticker...");
            _reachability_ticker->stop();
            _reachability_ticker.reset();
        }

        log::debug(logcat, "stopping nodedb events");
        node_db().cleanup();
    }

    void Router::stop_immediately()
    {
        if (!_is_running)
            return;
        if (_is_stopping)
            return;

        _is_stopping.store(true);
        if (log::get_level_default() != log::Level::off)
            log::reset_level(log::Level::info);

        log::warning(logcat, "Hard stopping router");
        llarp::sys::service_manager->stopping();
        _session_endpoint->stop();
        stop_outbounds();
        close();
    }

    void Router::stop()
    {
        if (!_is_running)
        {
            log::debug(logcat, "Stop called, but not running");
            return;
        }
        if (_is_stopping)
        {
            log::debug(logcat, "Stop called, but already stopping");
            return;
        }

        _is_stopping.store(true);

        if (auto level = log::get_level_default(); level > log::Level::info and level != log::Level::off)
            log::reset_level(log::Level::info);

        log::debug(logcat, "stopping service manager...");
        llarp::sys::service_manager->stopping();

        _session_endpoint->stop(true);

        if (not _is_service_node)
            _router_profiling.stop_save_ticker();

        _loop->call_later(200ms, [this] { cleanup(); });
    }

    quic::Address Router::listen_addr() const { return _listen_address; }

    const llarp::net::Platform& Router::net() const { return *llarp::net::Platform::Default_ptr(); }

}  // namespace llarp
