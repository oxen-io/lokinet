#pragma once

#include "route_poker.hpp"

#include <llarp/bootstrap.hpp>
#include <llarp/consensus/reachability_testing.hpp>
#include <llarp/constants/link_layer.hpp>
#include <llarp/contact/relay_contact.hpp>
#include <llarp/crypto/key_manager.hpp>
#include <llarp/handlers/session.hpp>
#include <llarp/handlers/tun_base.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/profiling.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/mem.hpp>
#include <llarp/util/str.hpp>
#include <llarp/util/time.hpp>
#include <llarp/vpn/platform.hpp>

#include <oxen/quic/loop.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace oxenmq
{
    class OxenMQ;
}

namespace llarp
{

    namespace link
    {
        struct Connection;
        class Endpoint;
        class Manager;
    }  // namespace link

    namespace rpc
    {
        class RPCServer;
        class RPCClient;
    }  // namespace rpc

    namespace quic = oxen::quic;

    inline constexpr std::chrono::milliseconds RC_UPDATE_INTERVAL{10min};

    // as we advance towards full mesh, we try to connect to this number per tick
    inline constexpr int FULL_MESH_ITERATION{1};

    // DISCUSS: ask tom and jason about this
    // how big of a time skip before we reset network state
    inline constexpr std::chrono::milliseconds NETWORK_RESET_SKIP_INTERVAL{1min};

    inline constexpr std::chrono::milliseconds REPORT_STATS_INTERVAL{10s};

    inline constexpr std::chrono::milliseconds DECOMM_WARNING_INTERVAL{5min};

    inline constexpr auto SERVICE_MANAGER_REPORT_INTERVAL{5s};

    class ContactDB;
    class NodeDB;

    class Router
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
        const std::shared_ptr<quic::Loop> _loop;
        std::chrono::steady_clock::time_point _next_explore_at;

        // path to write our self signed rc to
        std::filesystem::path our_rc_file;

        // our router contact
        LocalRC relay_contact;
        std::shared_ptr<oxenmq::OxenMQ> _omq{};

        std::atomic<bool> _is_stopping{false};
        std::atomic<bool> _is_running{false};

        // FIXME: we probably don't need two separate config options for this!
        bool _is_exit_node{_config.network.allow_exit || _config.exit.exit_enabled};

        bool _testing_disabled{_config.lokid.disable_testing};

        consensus::reachability_testing router_testing;

        std::optional<quic::Address> _public_address;  // public addr for relays
        quic::Address _listen_address;

        std::unique_ptr<handlers::SessionEndpoint> _session_endpoint;

        std::unique_ptr<link::Manager> _link_manager;
        link::Endpoint* _link_endpoint = nullptr;

        // These are only created in full platform mode (not embedded clients)
        std::shared_ptr<handlers::TunEPBase> _tun;
        std::shared_ptr<vpn::Platform> _vpn;
        std::shared_ptr<RoutePoker> _route_poker;

        std::promise<void> _close_promise;

        std::unique_ptr<ContactDB> _contact_db;
        std::unique_ptr<NodeDB> _node_db;

        std::shared_ptr<quic::Ticker> _loop_ticker;

        // Might not be set/used, depending on the platform:
        std::shared_ptr<quic::Ticker> _service_stat_ticker;
        std::shared_ptr<quic::Ticker> _reachability_ticker;

        std::shared_ptr<quic::Ticker> _gossip_ticker;

        std::chrono::milliseconds _started_at;
        std::chrono::milliseconds _last_stats_report{0s};
        std::chrono::milliseconds _next_dereg_warning{time_now_ms() + 15s};

        std::chrono::milliseconds _last_path_ping{0s};

        // These aren't actually shared, but we unique_ptr requires destructor visibility, which
        // embedded-only clients won't have as they don't compile any RPC code.
        std::shared_ptr<rpc::RPCServer> _rpc_server;
        std::shared_ptr<rpc::RPCClient> _rpc_client;

        bool registered_relays_received{false};

        Profiling _router_profiling;

        int min_client_outbounds{};
        std::atomic<bool> initial_client_connect_complete{false};

        bool should_report_stats(std::chrono::milliseconds now) const;

        std::string _stats_line();

        void report_stats();

        void save_rc();

        bool insufficient_peers() const;

        void init_logging();

        void process_routerconfig();

        void process_netconfig();

        void _relay_tick(std::chrono::milliseconds now);

        void _client_tick(std::chrono::milliseconds now);

        void tick();

        void start_tickers();

      public:
        path::PathContext path_context{*this};
        KeyManager key_manager;

        const bool is_service_node{_config.router.is_relay};

        bool is_fully_meshed() const;

        int client_outbounds_needed() const { return min_client_outbounds; }

        const std::shared_ptr<handlers::TunEPBase>& tun_endpoint() { return _tun; }

        // Returns the net Platform pointer, or nullptr if this is an embedded client.
        const llarp::net::Platform* net() const;

        const std::shared_ptr<vpn::Platform>& vpn_platform() const { return _vpn; }

        handlers::SessionEndpoint& session_endpoint() { return *_session_endpoint; }
        const handlers::SessionEndpoint& session_endpoint() const { return *_session_endpoint; }

        link::Manager& link_manager() { return *_link_manager; }
        const link::Manager& link_manager() const { return *_link_manager; }
        link::Endpoint& link_endpoint() { return *_link_endpoint; }
        const link::Endpoint& link_endpoint() const { return *_link_endpoint; }

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

        bool embedded() const { return _config.embedded(); }

        oxenmq::OxenMQ* omq() { return _omq.get(); }
        const oxenmq::OxenMQ* omq() const { return _omq.get(); }

        const std::shared_ptr<rpc::RPCClient>& rpc_client() const { return _rpc_client; }

        const Ed25519SecretKey& identity() const { return key_manager.identity_key; }

        const RouterID& local_rid() const { return key_manager.router_id(); }

        Profiling& router_profiling() { return _router_profiling; }

        quic::Loop& loop{*_loop};

        // Tiny event loop + thread for handling disk I/O jobs without affecting other loops.
        quic::Loop disk_loop;

        const LocalRC& rc() const { return relay_contact; }

        // Updates and re-signs the local RC and queues it for saving to disk.
        void update_rc();

        quic::Address listen_addr() const;

        nlohmann::json ExtractStatus() const;

        nlohmann::json ExtractSummaryStatus() const;

        void set_registered_relays(std::unordered_set<RouterID> relays);

        /// Return true if we are operating as a service node and have received a service node
        /// registered list from oxend.
        bool has_registered_relays() const;

        /// return true if we a registered service node (either active or decommissioned).
        bool appears_registered() const;

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

        bool is_exit_node() const;

        std::optional<std::string> OxendErrorState() const;

        void close();

        /// stop running the router logic gracefully
        void stop();

        /// non graceful stop router
        void stop_immediately();

        /// close all sessions and shutdown all links
        void stop_outbounds();

        void fetch_snode_identity();

        // bool is_bootstrap_node(RouterID rid) const;

        std::chrono::milliseconds now() const { return llarp::time_now_ms(); }

        void teardown();

        void cleanup();
    };
}  // namespace llarp
