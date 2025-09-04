#include "router.hpp"

#include <llarp/config/config.hpp>
#include <llarp/constants/platform.hpp>
#include <llarp/constants/proto.hpp>
#include <llarp/constants/version.hpp>
#include <llarp/contact/contactdb.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/link/link_manager.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/util/formattable.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/util/random.hpp>
#include <llarp/util/service_manager.hpp>
#include <llarp/util/time.hpp>

#include <oxen/log.hpp>

#include <chrono>

#ifndef LOKINET_EMBEDDED_ONLY
#include <llarp/handlers/tun.hpp>
#include <llarp/rpc/rpc_client.hpp>
#include <llarp/rpc/rpc_server.hpp>

#include <oxenmq/oxenmq.h>
#endif

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

static constexpr std::chrono::milliseconds ROUTER_TICK_INTERVAL{250ms};

namespace llarp
{
    static auto logcat = log::Cat("router");

    Router::Router(
        Config conf, std::shared_ptr<quic::Loop> loop, std::shared_ptr<vpn::Platform> vpnPlatform, std::promise<void> p)
        : _config{std::move(conf)},
          _loop{std::move(loop)},
          _next_explore_at{std::chrono::steady_clock::now()},
          _vpn{std::move(vpnPlatform)},
          _close_promise{std::move(p)},
          _contact_db{std::make_unique<ContactDB>(*this)},
          // TODO FIXME: what about non-testnet?  And do we really want a fixed random interval,
          // or do we want a randomized interval on each node's gossip?
          _last_tick{llarp::time_now_ms()}
    {
#ifndef LOKINET_EMBEDDED_ONLY
        // Not actually shared, but unique_ptr would require destructor visibility which
        // embedded-only won't have:
        _omq = std::make_shared<oxenmq::OxenMQ>();
        // for oxend, so we don't close the connection when syncing the registered relay (which can
        // exceed the defaut 1MB limit).
        _omq->MAX_MSG_SIZE = -1;
        if (_config.router.worker_threads > 0)
            _omq->set_general_threads(_config.router.worker_threads);
#endif

        init_logging();

        _loop->call_get([this] {
            log::debug(logcat, "Inside router loop, initializing router");
            configure();
            start();
        });
    }

    // Default, but we define it here because some of the unique_ptrs are for forward-declared types
    // in router.hpp which aren't available for destruction, but are available here.
    Router::~Router() = default;

    nlohmann::json Router::ExtractStatus() const
    {
        if (not _is_running)
            nlohmann::json{{"running", false}};

        nlohmann::json links;
        if (is_service_node)
        {
            auto [relays, out, in, pending, clients] = _link_endpoint->relay_connection_counts();
            links = {
                {"relays", relays},
                {"relays_in", in},
                {"relays_out", out},
                {"pending_out", pending},
                {"clients_in", clients}};
        }
        else
        {
            auto [nconns, pending] = _link_endpoint->client_connection_counts();
            links = {{"relays", nconns}, {"relays_out", nconns}, {"pending_out", pending}};
        }
        auto [rcs, rids, bootstraps] = node_db().db_stats();
        auto [npaths, nhops] = path_context.path_ctx_stats();
        auto [s_in, s_out_r, s_out_c, s_out_r_pending, s_out_c_pending] = _session_endpoint->session_stats();
        auto ccs = contact_db().num_ccs();

        return {
            {"instance",
             {{"id", local_rid().to_network_address(is_service_node).to_string()},
              {"running", true},
              {"relay", is_service_node}}},
            {"links", std::move(links)},
            {"sessions",
             {{"in", s_in},
              {"relay_out", s_out_r},
              {"client_out", s_out_c},
              {"relay_pending", s_out_r_pending},
              {"client_pending", s_out_c_pending}}},
            {"nodedb", {{"rcs", rcs}, {"rids", rids}}},
            {"contactdb", {{"ccs", ccs}}},
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

    void Router::start_tickers()
    {
        if (_tun)
            _tun->start_poller();

#ifndef LOKINET_EMBEDDED_ONLY
        if (!embedded())
            _service_stat_ticker = _loop->call_every(
                SERVICE_MANAGER_REPORT_INTERVAL, []() { sys::service_manager->report_periodic_stats(); });
#endif

        _node_db->start();
        _contact_db->start_tickers();
        _link_endpoint->start_tickers();

#ifndef LOKINET_EMBEDDED_ONLY
        if (is_service_node)
        {
            _rpc_client->start_pings();

            auto delay = uniform_duration_distribution{10s, 15s}(llarp::csrng);
            log::debug(logcat, "Delaying initial RC broadcast for {}", delay);
            loop.call_later(delay, [this] {
                update_rc();
                int count = _link_manager->gossip_rc(rc().to_remote());

                log::debug(logcat, "Sent initial RC to {} peers; starting RC regen ticker", count);
                _gossip_ticker = loop.call_every(RC_UPDATE_INTERVAL, [this] {
                    update_rc();
                    int count = _link_manager->gossip_rc(rc().to_remote());
                    log::debug(logcat, "Updated RC broadcast to {} peers", count);
                });
            });
        }
        else
#endif
        {
            // Resolve needed ONS values now that we have the necessary things prefigured
            _session_endpoint->resolve_sns_mappings();
        }
    }

    bool Router::is_fully_meshed() const { return link_endpoint().num_relay_conns() >= _node_db->num_rcs(); }

    void Router::fetch_snode_identity()
    {
        assert(is_service_node);
#ifndef LOKINET_EMBEDDED_ONLY

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
#endif
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

#ifndef NDEBUG
            std::string debug_pattern =
                log_type == log::Type::Print ? log::DEFAULT_PATTERN_COLOR : log::DEFAULT_PATTERN_MONO;
            // In a debug build replace YYYY-MM-DD with "t=THREADID"
            if (auto pos = debug_pattern.find("%Y-%m-%d"); pos != std::string::npos)
                debug_pattern = debug_pattern.substr(0, pos) + "t=%t" + debug_pattern.substr(pos + 8);
            else
                debug_pattern = "[t=%t] " + debug_pattern;
#endif

            log::clear_sinks();
            log::add_sink(
                log_type,
                log_type == log::Type::System ? "lokinet" : _config.logging.file
#ifndef NDEBUG
                ,
                debug_pattern
#endif
            );
        }

        log::extract_categories(_config.logging.levels).apply([](log::Level global_level) {
            // Override the global category to always print at info level, even when the
            // overall/default level is set higher.
            if (global_level > log::Level::info)
                log::set_level(log_global, log::Level::info);
        });

#ifndef LOKINET_EMBEDDED_ONLY
        // re-add rpc log sink if rpc enabled, else free it
        if (_config.api.enable_rpc_server and llarp::logRingBuffer)
            log::add_sink(llarp::logRingBuffer, llarp::log::DEFAULT_PATTERN_MONO);
        else
#endif
            llarp::logRingBuffer.reset();
    }

    void Router::process_routerconfig()
    {
        _client_target_outbounds = config().router.client_router_connections;

        if (is_service_node && embedded())
            throw std::runtime_error{"Invalid config: service node and embedded modes are incompatible!"};

        if (is_service_node)
        {
            auto paddr = _config.router.public_addr;
            if (!paddr)
                paddr = _config.links.public_addr;

            // Treat 0.0.0.0:0 as not specified:
            if (_config.links.listen_addr && _config.links.listen_addr->is_any_addr()
                && _config.links.listen_addr->is_any_port())
                _config.links.listen_addr.reset();

            // If our given public address has a port of 0 then set port to the listen port (if
            // one was explicitly given) or otherwise the default relay port.
            if (paddr)
            {
                if (paddr->is_any_port() && _config.links.listen_addr)
                    paddr->set_port(_config.links.listen_addr->port());
                if (paddr->is_any_port())
                    paddr->set_port(DEFAULT_RELAY_PORT);

                _public_address = paddr;
                if (!_public_address->is_public())
                    throw std::runtime_error{"Invalid public-ip: given IP address is not a public IP address"};
            }

            bool auto_detect = false;
            uint16_t auto_port = DEFAULT_RELAY_PORT;

            if (!_config.links.listen_addr)
            {
                if (_public_address)
                    // No listen address, but a public ip/port were given so use that
                    _listen_address = *_public_address;
                else
                    auto_detect = true;
            }
            else if (_config.links.listen_addr->is_any_addr())
            {
                assert(!_config.links.listen_addr->is_any_port());  // Should be assured from above
                // port given but not IP: if we have a public ip then use that, else go search
                if (paddr)
                    _listen_address = quic::Address{*paddr, _config.links.listen_addr->port()};
                else
                {
                    auto_detect = true;
                    auto_port = _config.links.listen_addr->port();
                }
            }
            else if (_config.links.listen_addr->is_any_port())
            {
                // IP given but not port.  If we have a public port use that, otherwise use the
                // default.
                _listen_address = *_config.links.listen_addr;
                _listen_address.set_port(paddr ? paddr->port() : DEFAULT_RELAY_PORT);
            }
            else
            {
                assert(_config.links.listen_addr->is_addressable());
                _listen_address = *_config.links.listen_addr;
            }

            if (auto_detect)
            {
                assert(net());
                if (auto maybe_addr = net()->get_best_public_address(true, auto_port))
                    _public_address = _listen_address = std::move(*maybe_addr);
                else
                    throw std::runtime_error{
                        "Unable to determine a public IP on this system; you likely need to set "
                        "[router]:public-ip/public-port config settings"};
            }

            if (!_public_address)
            {
                if (_listen_address.is_public())
                    _public_address = _listen_address;
                else
                    throw std::runtime_error{"When listening on a non-public IP, public-ip must be specified"};
            }

            if (_listen_address == *_public_address)
                log::info(logcat, "Using {} for Lokinet communications", _listen_address);
            else if (!_listen_address.is_public())
                log::info(
                    logcat,
                    "Listening on private address {} with publicly reachable address {}",
                    _listen_address,
                    *_public_address);
            else
                // Binding to a different public IP/port than we actually advertise in RCs is not
                // allowed, as it is almost certainly a configuration error (e.g. such as moving the
                // config from one server to another and updating only one of the two values).
                throw std::runtime_error{
                    "public-ip/port ({}) and listen address ({}) are both public addresses but do not match!"_format(
                        _public_address, _listen_address)};
        }
        else  // Not a service node:
        {
            _listen_address = _config.links.listen_addr.value_or(DEFAULT_CLIENT_ADDR);

            log::info(logcat, "Using {} for Lokinet communications", _listen_address);
        }

        RelayContact::BLOCK_BOGONS = _config.router.block_bogons;
    }

    void Router::process_netconfig()
    {
        auto& conf = _config.network;

        if (!embedded())
        {
            assert(net());

            if (!conf._if_name)
                conf._if_name = net()->find_free_tun();

            if (!(conf._local_ip_net && conf._local_ip_net->ip.addr))
            {
                if (auto maybe = net()->find_free_ipv4_net(conf._local_ip_net ? conf._local_ip_net->mask : 16))
                    conf._local_ip_net = std::move(*maybe);
                else
                    throw std::runtime_error("cannot find free IPv4 address range!");
            }
            log::info(logcat, "Lokinet IPv4 local network is {}", *conf._local_ip_net);

            if (conf.enable_ipv6)
            {
                if (!conf._local_ipv6_net || (!conf._local_ipv6_net->ip.hi && !conf._local_ipv6_net->ip.lo))
                {
                    if (auto maybe = net()->find_free_ipv6_net(conf._local_ipv6_net ? conf._local_ipv6_net->mask : 64))
                        conf._local_ipv6_net = std::move(*maybe);
                    else
                        throw std::runtime_error("cannot find free IPv6 address range!");
                }
                log::info(logcat, "Lokinet IPv6 local network is {}", *conf._local_ipv6_net);
                log::warning(
                    logcat,
                    "Lokinet IPv6 support is a work-in-progress and unsupported; enabling it is not recommended");
            }

            // Make sure any reserved addresses are within our local network range:
            std::erase_if(conf._reserved_local_ipv4, [&conf](const auto& addr_ip) {
                return !conf._local_ip_net->contains(addr_ip.second);
            });
            if (conf._local_ipv6_net)
                std::erase_if(conf._reserved_local_ipv6, [&conf](const auto& addr_ip) {
                    return !conf._local_ipv6_net->contains(addr_ip.second);
                });
        }

        // parse strict-connet pubkeys
        if (auto& conf_edges = conf.pinned_edges; not conf_edges.empty())
        {
            if (is_service_node)
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

            if (_client_target_outbounds > n_edges)
            {
                _client_target_outbounds = n_edges;
                log::warning(logcat, "Minimum router connections reduced to {} to match strict-connect edges", n_edges);
            }
        }
        else
            log::debug(
                logcat,
                "Local client configured to maintain {} router connections at minimum",
                _client_target_outbounds);

        if (not _client_target_outbounds)
            throw std::runtime_error{"Client must be configured to have at least 1 outbound router connection!"};
    }

    void Router::configure()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
#ifndef LOKINET_EMBEDDED_ONLY
        if (!embedded())
            sys::service_manager->starting();
#endif

        if (_is_exit_node and is_service_node)
            throw std::runtime_error{
                "Lokinet cannot simultaneously operate as a service node and client-operated exit node service!"};

        if (_config.lokid.disable_testing && netid() == NetID::MAINNET)
            throw std::runtime_error{"Error: reachability testing can only be disabled on testnet!"};

        auto net_id = netid();
        log::log(logcat, net_id == NetID::MAINNET ? log::Level::debug : log::Level::warn, "Network ID is {}", net_id);

        log::trace(logcat, "Configuring router...");

        log::info(log_global, "Operating as a Lokinet {}", is_service_node ? "relay (service node)" : "client");

#ifndef LOKINET_EMBEDDED_ONLY
        _omq->log_level(oxenlog_to_omq_level(log::get_level_default()));

        if (is_service_node)
        {
            log::debug(logcat, "Starting RPC client");
            _rpc_client = std::make_shared<rpc::RPCClient>(*_omq, *this);
        }

        if (_config.api.enable_rpc_server)
        {
            log::debug(logcat, "Starting RPC server");
            //
            _rpc_server = std::make_shared<rpc::RPCServer>(*_omq, *this);
        }

        log::debug(logcat, "Starting OMQ server");
        _omq->start();

        if (is_service_node)
        {
            log::trace(logcat, "RPC client connecting to RPC bind address");
            _rpc_client->connect_async(oxenmq::address(_config.lokid.rpc_addr));
        }
#endif

        log::debug(logcat, "Initializing key manager");

        if (is_service_node)
            fetch_snode_identity();
        else
            key_manager = KeyManager{_config, is_service_node};

        log::trace(logcat, "Initializing from configuration");

        process_routerconfig();

        if (is_service_node)
            log::info(
                log_global,
                "Lokinet relay listening on {}{}",
                _listen_address,
                _public_address ? " with public address {}"_format(*_public_address) : "");
        else
            log::info(log_global, "Lokinet client connection using {}", _listen_address);

        // We process the relevant netconfig values (ip_range, address, and ip) here; in case the range or interface
        // is bad, we search for a free one and set it BACK into the config. Every subsequent object configuring
        // using the NetworkConfig (ex: tun/null, exit::Handler, etc) will have processed values
        process_netconfig();

        _node_db = std::make_unique<NodeDB>(*this);

        relay_contact = {identity(), is_service_node and _public_address ? *_public_address : _listen_address, netid()};

        if (is_service_node and not relay_contact.addr().is_public())
        {
            auto err =
                "Router is configured as relay but '{}' is not a public IP; perhaps"
                " you need to specify the [router]:public-ip/public-port settings?"_format(relay_contact.addr());
            log::critical(logcat, "{}", err);
            throw std::runtime_error{err};
        }

        _session_endpoint = std::make_unique<handlers::SessionEndpoint>(*this);

        log::debug(logcat, "Creating QUIC link manager");
        _link_manager = std::make_unique<link::Manager>(*this);
        _link_endpoint = &_link_manager->endpoint;

        if (!embedded())
        {
#ifdef LOKINET_EMBEDDED_ONLY
            log::critical(logcat, "This lokinet build only supports embedded configurations!");
            throw std::runtime_error{"This lokinet build only supports embedded configurations!"};
#else
            log::debug(logcat, "Initializing TUN device");
            auto tun = _loop->make_shared<handlers::TunEndpoint>(*this);
            tun->setup_dns();

            log::info(
                log_global, "Lokinet internal network: {} on device {}", tun->get_ipv4_network(), tun->get_if_name());

            _tun = std::move(tun);
#endif
        }
        else
            log::debug(logcat, "Not initializing TUN device; running as an embedded client");
    }

    bool Router::is_exit_node() const { return _is_exit_node; }

    bool Router::insufficient_peers() const
    {
        constexpr int KnownPeerWarningThreshold = 5;
        return node_db().num_rcs() < KnownPeerWarningThreshold;
    }

    std::optional<std::string> Router::OxendErrorState() const
    {
        // If we're in the registered list then we *should* be establishing connections to other
        // routers, so if we have almost no peers then something is almost certainly wrong.
        if (insufficient_peers() and not _testing_disabled)
            return "too few peer connections; lokinet is not adequately connected to the network";
        return std::nullopt;
    }

    bool Router::appears_registered() const { return is_service_node and node_db().is_registered(local_rid()); }

    void Router::update_rc()
    {
        relay_contact.resign();
        save_rc();
    }

    void Router::save_rc()
    {
        disk_loop.call([this] {
            log::info(logcat, "Saving RC file to {}", our_rc_file);
            relay_contact.write(our_rc_file);
        });
    }

    bool Router::should_report_stats(std::chrono::milliseconds now) const
    {
        return now >= _started_at + 10s
            and now >= _last_stats_report
                + (log::get_level(logcat) <= log::Level::debug ? REPORT_STATS_INTERVAL_DEBUG : REPORT_STATS_INTERVAL);
    }

    std::string Router::_stats_line(std::chrono::milliseconds now) const
    {
        using namespace fmt::literals;
        auto [rcs, rids, bs] = _node_db->db_stats();
        auto [s_in, s_out_r, s_out_c, s_out_r_pending, s_out_c_pending] = _session_endpoint->session_stats();
        auto [in_paths, out_r_paths, out_c_paths] = _session_endpoint->path_stats(now);
        if (is_service_node)
        {
            auto [relays, rout, rin, rpending, clients] = link_endpoint().relay_connection_counts();
            return fmt::format(
                "relays: {relays} conns ({rin}↓, {rout}↑{pending}), RC/RIDs: {rcs}/{rids}; "
                "{clients} clients; sessions: {sess_in}↓; paths: {paths_in}",

                "relays"_a = relays,
                "rin"_a = rin,
                "rout"_a = rout,
                "pending"_a = rpending ? ", {} pending"_format(rpending) : "",
                "clients"_a = clients,
                "sess_in"_a = s_in,
                "paths_in"_a = in_paths,
                "rcs"_a = rcs,
                "rids"_a = rids);
        }

        auto [nconns, npending] = link_endpoint().client_connection_counts();
        return fmt::format(
            "conns: {conns}; sessions: {sess_in}↓/{sess_out_c}↑(c)/{sess_out_r}↑(r); "
            "paths: {paths_in}↓/{paths_out}↑, {path_percent:.1f}% of {path_total} attempts; "
            "RC/RIDs: {rcs}/{rids}",

            "conns"_a = nconns + npending,
            "sess_in"_a = s_in,
            "sess_out_c"_a = s_out_c,
            "sess_out_r"_a = s_out_r,
            "paths_in"_a = in_paths,
            "paths_out"_a = out_r_paths + out_c_paths,
            "path_percent"_a = (double)path_builds.success * 100.0 / (double)path_builds.attempts,
            "path_total"_a = path_builds.attempts,
            "rcs"_a = rcs,
            "rids"_a = rids);
    }

    void Router::report_stats()
    {
        const auto now = llarp::time_now_ms();

        log::info(log_global, "Local {}: {}", is_service_node ? "Relay" : "Client", _stats_line(now));

        _last_stats_report = now;

        oxen::log::flush();
    }

    std::string Router::status_line()
    {
        auto now = llarp::time_now_ms();
        return "v{} {}: {}"_format(llarp::LOKINET_VERSION_FULL, is_service_node ? "relay" : "client", _stats_line(now));
    }

    void Router::_relay_tick([[maybe_unused]] std::chrono::milliseconds now)
    {
        assert(_config.relay());
#ifndef LOKINET_EMBEDDED_ONLY
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (should_report_stats(now))
            report_stats();

        bool registered = appears_registered();

        if (now >= _next_dereg_warning)
        {
            if (not registered)
            {
                // complain about being deregistered/decommed
                log::error(logcat, "We are running as a relay but are not a registered service node");
                _next_dereg_warning = now + DECOMM_WARNING_INTERVAL;
            }
            else if (insufficient_peers())
            {
                log::error(
                    logcat, "We are an active service node, but have too few ({}) known peers!", node_db().num_rcs());
                _next_dereg_warning = now + DECOMM_WARNING_INTERVAL;
            }
        }

        if (registered and link_endpoint().num_relay_conns(/*include_pending=*/true) < node_db().num_rcs())
        {
            log::debug(
                logcat, "Service Node connecting to {} random routers to achieve full mesh", FULL_MESH_ITERATION);
            _link_manager->connect_to_keep_alive(FULL_MESH_ITERATION);
        }

        path_context.expire_hops(now);
#endif
    }

    void Router::_client_tick(std::chrono::milliseconds now)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        _router_profiling.tick();

        if (now > _last_path_ping + 1s)
        {
            _last_path_ping = now;
            _session_endpoint->ping_paths(now);
        }

        if (should_report_stats(now))
            report_stats();

        // TODO: make "use_pinned_edges" boolean to only connect to pinned edges
        // if we need more sessions to routers we shall connect out to others
        if (int n_conns = link_endpoint().num_relay_conns(/*include_pending=*/true); n_conns < _client_target_outbounds)
        {
            auto num_needed = _client_target_outbounds - n_conns;

            log::debug(
                logcat,
                "Client connecting to {} random routers to keep alive (current:{}, needed:{})",
                num_needed,
                n_conns,
                _client_target_outbounds);
            _link_manager->connect_to_keep_alive(num_needed);
        }

        _session_endpoint->tick(now);
    }

    void Router::tick()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (_is_stopping)
        {
            log::debug(logcat, "Router is stopping; exiting ::tick()...");
            return;
        }

        const auto now = llarp::time_now_ms();

        if (const auto delta = now - _last_tick;
            _last_tick != 0s and (delta > NETWORK_RESET_SKIP_INTERVAL || delta < -NETWORK_RESET_SKIP_INTERVAL))
        {
            // TODO: this, if needed?
            // we detected a time skip into the futre, thaw the network
            log::error(logcat, "Timeskip of {}ms detected, resetting network state!", delta.count());
        }

        if (is_service_node)
            _relay_tick(now);
        else
            _client_tick(now);

        // update tick timestamp
        _last_tick = llarp::time_now_ms();
    }

    void Router::start()
    {
        log::debug(logcat, "{} called", __PRETTY_FUNCTION__);

        if (is_service_node)
        {
            save_rc();

            log::debug(logcat, "Router accepting transit traffic");
            path_context.allow_transit();

            // relays do not use profiling
            _router_profiling.disable();
        }
        else if (netid() == NetID::MAINNET and _config.network.enable_profiling)
        {
            _router_profiling._profile_file = _config.router.data_dir / "profiles.dat";

            log::debug(logcat, "Router profiling enabled");
            if (not std::filesystem::exists(_router_profiling._profile_file))
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

        log::debug(logcat, "Starting Router main tick interval");
        _loop_ticker = _loop->call_every(ROUTER_TICK_INTERVAL, [this] { tick(); });

        _started_at = now();

#ifndef LOKINET_EMBEDDED_ONLY
        if (is_service_node and not _testing_disabled)
        {
            // TODO FIXME: this reachability testing implementation is definitely not right: it
            // involves establishing a new connection to the remote, and then closing it, but that
            // definitely isn't right because the new connection will replace the existing one at
            // the remote end, *both* of which can break in-flight data through that connection.
            //
            // Perhaps we should just issue a ping request on the current connection, and call it a
            // day?
            // - what about if the IP/port of the current connection doesn't match the RC?
            //
            // Perhaps we should always establish a new connection to the RC, but *not* using our
            // current keys (i.e. using a client ALPN), not putting it into link endpoint, and close
            // it as soon as we get a ping?
            log::critical(logcat, "Reachability testing disabled - TODO FIXME");
#if 0   // FIXME
            log::debug(logcat, "Creating reachability testing ticker...");
            _reachability_ticker = _loop->call_every(consensus::REACHABILITY_TESTING_TIMER_INTERVAL, [this] {

                // dont run tests if we are not running or we are stopping
                if (not _is_running)
                    return;

                // Don't run testing if we are not a registered service node, because other service
                // nodes in that case wouldn't allow our connection.
                if (not appears_registered())
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
            });
#endif  // FIXME
        }
#endif

        start_tickers();
        _is_running = true;

#ifndef LOKINET_EMBEDDED_ONLY
        if (!embedded())
            llarp::sys::service_manager->ready();
#endif

        log::info(
            log_global,
            "{} started @ {}",
            is_service_node ? "Relay" : "Client",
            local_rid().to_network_address(is_service_node));

        // Fire a tick right now to start making connections immediately (rather than waiting until
        // the first tick):
        tick();
    }

    std::chrono::milliseconds Router::Uptime() const
    {
        const std::chrono::milliseconds _now = now();
        if (_started_at > 0s && _now > _started_at)
            return _now - _started_at;
        return 0s;
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

        if (_is_stopping.exchange(true))
            return;  // Lost a race with something else trying to stop

        _loop->call([this] {
#ifndef LOKINET_EMBEDDED_ONLY
            if (!embedded())
            {
                log::debug(logcat, "stopping service manager...");
                llarp::sys::service_manager->stopping();
            }
#endif

            _session_endpoint->stop(true);

            if (not is_service_node)
                _router_profiling.stop_save_ticker();

            log::debug(logcat, "closing all connections");
            _link_manager->stop();

            auto rv = _loop_ticker->stop();
            log::debug(logcat, "router loop ticker stopped {}successfully!", rv ? "" : "un");
            _loop_ticker.reset();

            rv = _service_stat_ticker->stop();
            log::debug(logcat, "service stat ticker stopped {}successfully!", rv ? "" : "un");
            _service_stat_ticker.reset();

            if (_reachability_ticker)
            {
                log::debug(logcat, "clearing reachability ticker...");
                _reachability_ticker->stop();
                _reachability_ticker.reset();
            }

            log::debug(logcat, "stopping nodedb events");
            node_db().cleanup();

            // Submit a dummy job to the disk loop that we wait on to ensure that we've cleared out
            // any pending disk write jobs.
            log::debug(logcat, "flushing disk loop jobs");
            disk_loop.call_get([] {});

            log::debug(logcat, "cleaning up link_manager");
            _link_endpoint = nullptr;
            _link_manager.reset();

            if (_router_close_cb)
                _router_close_cb();

            _is_running.store(false);

            _omq.reset();

            _close_promise.set_value();
            log::info(log_global, "Lokinet has stopped");
        });
    }

    const llarp::net::Platform* Router::net() const
    {
#ifndef LOKINET_EMBEDDED_ONLY
        if (!embedded())
            return llarp::net::Platform::Default_ptr();
#endif
        return nullptr;
    }

}  // namespace llarp
