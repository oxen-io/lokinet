#pragma once

#include "route_poker.hpp"

#include <llarp/bootstrap.hpp>
#include <llarp/consensus/reachability_testing.hpp>
#include <llarp/constants/link_layer.hpp>
#include <llarp/contact/relay_contact.hpp>
#include <llarp/crypto/key_manager.hpp>
#include <llarp/handlers/session.hpp>
#include <llarp/handlers/tun.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/profiling.hpp>
#include <llarp/rpc/rpc_client.hpp>
#include <llarp/rpc/rpc_server.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/mem.hpp>
#include <llarp/util/service_manager.hpp>
#include <llarp/util/str.hpp>
#include <llarp/util/time.hpp>
#include <llarp/vpn/platform.hpp>

#include <oxen/quic/loop.hpp>
#include <oxenmq/address.h>

#include <functional>
#include <memory>
#include <vector>

namespace llarp
{
    namespace link
    {
        struct Connection;
    }  // namespace link

    namespace quic = oxen::quic;

    class LinkManager;

    /// number of routers to publish to
    inline constexpr size_t INTROSET_RELAY_REDUNDANCY{2};

    /// number of dht locations handled per relay
    // DISCUSS: do we need this??
    // inline constexpr size_t INTROSET_REQS_PER_RELAY{2};
    // inline constexpr size_t INTROSET_STORAGE_REDUNDANCY{(INTROSET_RELAY_REDUNDANCY * INTROSET_REQS_PER_RELAY)};

    // TESTNET: these constants are shortened for testing purposes
    inline uniform_duration_distribution TESTNET_GOSSIP_INTERVAL{5min, 5min + 30s};
    inline constexpr std::chrono::milliseconds RC_UPDATE_INTERVAL{5min};
    inline constexpr std::chrono::milliseconds INITIAL_ATTEMPT_INTERVAL{30s};
    // as we advance towards full mesh, we try to connect to this number per tick
    inline constexpr int FULL_MESH_ITERATION{1};
    inline constexpr std::chrono::milliseconds ROUTERID_UPDATE_INTERVAL{1h};

    // DISCUSS: ask tom and jason about this
    // how big of a time skip before we reset network state
    inline constexpr std::chrono::milliseconds NETWORK_RESET_SKIP_INTERVAL{1min};

    inline constexpr std::chrono::milliseconds REPORT_STATS_INTERVAL{10s};

    inline constexpr std::chrono::milliseconds DECOMM_WARNING_INTERVAL{5min};

    inline constexpr auto SERVICE_MANAGER_REPORT_INTERVAL{5s};

    class ContactDB;
    class NodeDB;

    class Router : std::enable_shared_from_this<Router>
    {
      public:
        // Starts Lokinet immediately upon construction.
        explicit Router(
            Config conf,
            std::shared_ptr<quic::Loop> loop,
            std::shared_ptr<vpn::Platform> vpnPlatform,
            std::promise<void> p);

        ~Router();

      private:
        // Internal functions called during construction:
        void configure();
        void start();

        Config _config;
        std::shared_ptr<quic::Loop> _loop;
        std::chrono::steady_clock::time_point _next_explore_at;

        // path to write our self signed rc to
        fs::path our_rc_file;

        // our router contact
        LocalRC relay_contact;
        std::unique_ptr<oxenmq::OxenMQ> _omq{};
        path::BuildLimiter _pathbuild_limiter;

        std::atomic<bool> _is_stopping{false};
        std::atomic<bool> _is_running{false};

        bool _is_service_node{_config.router.is_relay};

        // FIXME: we probably don't need two separate config options for this!
        bool _is_exit_node{_config.network.allow_exit || _config.exit.exit_enabled};

        bool _testing_disabled{_config.lokid.disable_testing};
        bool _bootstrap_seed{false};

        consensus::reachability_testing router_testing;

        std::optional<quic::Address> _public_address;  // public addr for relays
        quic::Address _listen_address;

        std::unique_ptr<handlers::SessionEndpoint> _session_endpoint;

        std::unique_ptr<LinkManager> _link_manager;

        // Only created in full client and relay instances (not embedded clients)
        std::shared_ptr<handlers::TunEndpoint> _tun;

        std::promise<void> _close_promise;

        std::shared_ptr<vpn::Platform> _vpn;
        std::unique_ptr<RoutePoker> _route_poker;

        std::unique_ptr<ContactDB> _contact_db;
        std::unique_ptr<NodeDB> _node_db;

        std::shared_ptr<quic::Ticker> _loop_ticker;
        std::shared_ptr<quic::Ticker> _systemd_ticker;
        std::shared_ptr<quic::Ticker> _reachability_ticker;

        const oxenmq::TaggedThreadID _disk_thread;

        std::chrono::milliseconds _started_at;
        std::chrono::milliseconds _last_stats_report{0s};
        std::chrono::milliseconds _next_decomm_warning{time_now_ms() + 15s};

        std::chrono::milliseconds _last_path_ping;

        std::unique_ptr<rpc::RPCServer> _rpc_server;

        std::unique_ptr<rpc::RPCClient> _rpc_client;
        bool whitelist_received{false};

        oxenmq::address rpc_addr;
        Profiling _router_profiling;

        int min_client_outbounds{};
        std::atomic<bool> initial_client_connect_complete{false};

        bool should_report_stats(std::chrono::milliseconds now) const;

        std::string _stats_line();

        void report_stats();

        void save_rc();

        bool insufficient_peers() const;

        void init_logging();

        void init_tun();

        void init_bootstrap();

        void process_routerconfig();

        void process_netconfig();

        std::chrono::milliseconds _gossip_interval;

        void _relay_tick(std::chrono::milliseconds now);

        void _client_tick(std::chrono::milliseconds now);

        void tick();

        void start_tickers();

      public:
        path::PathContext path_context{*this};
        KeyManager key_manager;

        bool is_fully_meshed() const;

        bool using_tun_if() const { return static_cast<bool>(_tun); }

        bool is_bootstrap_seed() const { return _bootstrap_seed; }

        int client_outbounds_needed() const { return min_client_outbounds; }

        std::unordered_set<RouterID> get_current_remotes() const;

        void for_each_connection(std::function<void(const RouterID&, link::Connection&)> func);

        const std::shared_ptr<handlers::TunEndpoint>& tun_endpoint() const { return _tun; }

        handlers::SessionEndpoint& session_endpoint() { return *_session_endpoint; }
        const handlers::SessionEndpoint& session_endpoint() const { return *_session_endpoint; }

        LinkManager& link_manager() { return *_link_manager; }
        const LinkManager& link_manager() const { return *_link_manager; }

        const Config& config() const { return _config; }

        ContactDB& contact_db()
        {
            assert(_contact_db);
            return *_contact_db;
        }
        const ContactDB& contact_db() const
        {
            assert(_contact_db);
            return *_contact_db;
        }

        NodeDB& node_db()
        {
            assert(_node_db);
            return *_node_db;
        }
        const NodeDB& node_db() const
        {
            assert(_node_db);
            return *_node_db;
        }

        NetID netid() const { return _config.router.net_id; }

        path::BuildLimiter& pathbuild_limiter() { return _pathbuild_limiter; }

        const llarp::net::Platform& net() const;

        oxenmq::OxenMQ& omq() { return *_omq; }
        const oxenmq::OxenMQ& omq() const { return *_omq; }

        const std::unique_ptr<rpc::RPCClient>& rpc_client() const { return _rpc_client; }

        const Ed25519SecretKey& identity() const { return key_manager.identity_key; }

        const RouterID& local_rid() const { return key_manager.router_id(); }

        Profiling& router_profiling() { return _router_profiling; }

        const std::shared_ptr<quic::Loop>& loop() const { return _loop; }

        vpn::Platform* vpn_platform() const { return _vpn.get(); }

        std::chrono::milliseconds gossip_interval() const { return _gossip_interval; }

        const LocalRC& rc() const { return relay_contact; }

        // Updates and resigns the local RC, saves it, then returns it converted to a RemoteRC for
        // gossipping.
        RemoteRC update_rc_for_gossiping();

        quic::Address listen_addr() const;

        nlohmann::json ExtractStatus() const;

        nlohmann::json ExtractSummaryStatus() const;

        void queue_work(std::function<void()> func);
        void queue_disk_io(std::function<void()> func);

        const std::unordered_set<RouterID>& get_whitelist() const;

        void set_router_whitelist(const std::vector<RouterID>& whitelist);

        /// Return true if we are operating as a service node and have received a service node
        /// whitelist
        bool has_whitelist() const;

        /// return true if we look like we are a decommissioned service node
        bool appears_decommed() const;

        /// return true if we look like we are a registered, fully-staked service node (either
        /// active or decommissioned).  This condition determines when we are allowed to (and
        /// attempt to) connect to other peers when running as a service node.
        bool appears_funded() const;

        /// return true if we a registered service node; not that this only requires a partial
        /// stake, and does not imply that this service node is *active* or fully funded.
        bool appears_registered() const;

        /// return true if we look like we are allowed and able to test other routers
        bool can_test_routers() const;

        std::chrono::milliseconds Uptime() const;

        std::chrono::milliseconds _last_tick;

        std::function<void(void)> _router_close_cb;

        void set_router_close_cb(std::function<void(void)> hook) { _router_close_cb = hook; }

        bool looks_alive() const { return now() - _last_tick <= 30s; }

        // RoutePoker& route_poker() { return *_route_poker; }
        // const RoutePoker& route_poker() const { return *_route_poker; }

        std::string status_line();

        bool is_running() const { return _is_running; }

        bool is_stopping() const { return _is_stopping; }

        bool is_service_node() const;

        bool is_exit_node() const;

        std::optional<std::string> OxendErrorState() const;

        void close();

        /// stop running the router logic gracefully
        void stop();

        /// non graceful stop router
        void stop_immediately();

        /// close all sessions and shutdown all links
        void stop_outbounds();

        void persist_connection_until(const RouterID& remote, std::chrono::milliseconds until);

        void fetch_snode_identity();

        bool send_data_message(const RouterID& remote, std::string payload);

        bool send_control_message(
            const RouterID& remote,
            std::string endpoint,
            std::string body,
            std::function<void(quic::message)> func = nullptr);

        // bool is_bootstrap_node(RouterID rid) const;

        std::chrono::milliseconds now() const { return llarp::time_now_ms(); }

        /// count the number of unique service nodes connected via pubkey
        size_t num_router_connections(bool active_only = true) const;

        /// count the number of unique clients connected by pubkey
        size_t num_client_connections() const;

        void teardown();

        void cleanup();
    };
}  // namespace llarp
