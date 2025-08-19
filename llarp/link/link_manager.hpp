#pragma once

#include "connection.hpp"

#include <llarp/address/address.hpp>
#include <llarp/constants/path.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/messages/common.hpp>
#include <llarp/path/transit_hop.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/compare_ptr.hpp>
#include <llarp/util/decaying_hashset.hpp>

#include <oxen/quic/btstream.hpp>
#include <oxen/quic/connection.hpp>
#include <oxen/quic/endpoint.hpp>
#include <oxen/quic/format.hpp>
#include <oxen/quic/gnutls_crypto.hpp>
#include <oxen/quic/loop.hpp>
#include <oxen/quic/opt.hpp>

#include <atomic>
#include <unordered_map>

namespace llarp
{
    class LinkManager;
    class NodeDB;

    using quic::connection_closed_callback;
    using quic::connection_established_callback;

    inline constexpr auto RELAY_KEEP_ALIVE = 10s;
    inline constexpr auto CLIENT_KEEP_ALIVE = 10s;

    inline const auto RELAY_ALPN = "Lokinet_R"s;
    inline const auto CLIENT_ALPN = "Lokinet_C"s;

    namespace link
    {
        struct Connection;

        struct Endpoint
        {
            Endpoint(std::shared_ptr<quic::Endpoint> ep, LinkManager& lm);

            std::shared_ptr<quic::Endpoint> endpoint;
            LinkManager& link_manager;
            Router& router;

            /** Connection containers:
                - service_conns: holds all connections where the remote (from the perspective
                  of the local lokinet instance) is a service node. This means all relay to
                  relay connections are held here; clients will also hold their connections to
                  relays here as well
                - client_conns: holds all connections where the remote is a client. This is only
                  used by service nodes to store their client connections
            */
            std::unordered_map<RouterID, std::shared_ptr<link::Connection>> service_conns;
            std::unordered_map<RouterID, std::shared_ptr<link::Connection>> client_conns;

            std::shared_ptr<link::Connection> get_conn(const RouterID&) const;

            std::shared_ptr<link::Connection> get_service_conn(const RouterID&) const;

            bool have_conn(const RouterID& remote) const;

            bool have_client_conn(const RouterID& remote) const;

            bool have_service_conn(const RouterID& remote) const;

            std::tuple<size_t, size_t, size_t, size_t> connection_stats() const;

            size_t num_client_conns() const;

            size_t num_router_conns(bool active_only = true) const;

            bool establish_connection(
                quic::RemoteAddress remote,
                RouterID rid,
                connection_established_callback on_open = nullptr,
                connection_closed_callback on_close = nullptr);

            bool establish_and_send_control(const RemoteRC& rc, std::function<void(quic::BTRequestStream&)> send_hook);

            bool establish_and_send(
                quic::RemoteAddress remote,
                const RouterID& rid,
                std::string endpoint,
                std::vector<std::byte> body,
                std::function<void(quic::message)> func = nullptr);

            void for_each_connection(std::function<void(const RouterID&, link::Connection&)> func);

            void for_each_service_conn(
                std::function<void(const RouterID&, link::Connection&)> func, bool active_only = true);

            void close_connection(const RouterID& rid);

            void close_all();
        };
    }  // namespace link

    class Router;

    class LinkManager
    {
      public:
        explicit LinkManager(Router& r);

        void send_control_message(
            const RouterID& remote,
            std::string endpoint,
            std::vector<std::byte> body,
            std::function<void(quic::message)> = nullptr);

        bool send_data_message(const RouterID& remote, std::vector<std::byte> data);

        Router& router;

      private:
        friend struct link::Endpoint;
        friend class NodeDB;

        util::DecayingHashSet<RouterID> clients{path::MAX_LIFETIME};

        quic::Address addr;

        std::unique_ptr<quic::Loop> quic_loop;
        std::shared_ptr<quic::GNUTLSCreds> tls_creds;
        std::unique_ptr<link::Endpoint> ep;

        std::atomic<bool> is_stopping;

        std::shared_ptr<quic::BTRequestStream> make_control(quic::Connection& conn, const RouterID& rid);

        void on_inbound_conn(std::shared_ptr<quic::Connection> conn);

        void on_outbound_conn(RouterID id);

        void on_conn_open(quic::Connection& conn);

        void on_conn_closed(quic::Connection& conn, uint64_t ec);

        std::shared_ptr<quic::Endpoint> startup_endpoint();

        void register_commands(quic::BTRequestStream& s, const RouterID& rid, bool client_only = false);

      public:
        const quic::Address& local() { return addr; }

        bool have_connection_to(const RouterID& remote) const;

        bool have_service_connection_to(const RouterID& remote) const;

        bool have_client_connection_to(const RouterID& remote) const;

        void test_reachability(const RouterID& rid, connection_established_callback, connection_closed_callback);

        void connect_to(
            const RemoteRC& rc, connection_established_callback = nullptr, connection_closed_callback = nullptr);

        void connect_and_send(
            const RouterID& router,
            std::string endpoint,
            std::vector<std::byte> body,
            std::function<void(quic::message)> func = nullptr);

        void connect_and_send(const RouterID& router, std::function<void(quic::BTRequestStream&)> send_hook);

        void close_connection(RouterID rid);

        void stop();

        void close_all_links();

        std::tuple<size_t, size_t, size_t, size_t> connection_stats() const;

        size_t get_num_connected_routers(bool active_only = true) const;

        size_t get_num_connected_clients() const;

        nlohmann::json extract_status() const;

        std::unordered_set<RouterID> get_current_remotes() const;

        void for_each_connection(std::function<void(const RouterID&, link::Connection&)> func);

        // Attempts to connect to a number of random routers.
        //
        // This will try to connect to *up to* num_conns routers, but will not
        // check if we already have a connection to any of the random set, as making
        // that thread safe would be slow...I think.
        void connect_to_keep_alive(int num_conns);

        /// always maintain this many client connections to other routers
        int client_router_connections = 4;

        // Sends the given RC to all our relay peers, excluding connections to the RC pubkey itself,
        // and (if not-nullptr) the given quic connection.  Returns the number of relay connections
        // we sent it to.
        int gossip_rc(const RemoteRC& rc, const quic::ConnectionID* sender = nullptr);

      private:
        void handle_gossip_rc(quic::message);

        void fetch_rcs(const RouterID& source, std::vector<std::byte> payload, std::function<void(quic::message)> func);

        void fetch_router_ids(const RouterID& via, std::function<void(quic::BTRequestStream&)> send_hook);
        void handle_fetch_router_ids(quic::message);

        void fetch_bootstrap_rcs(
            const RemoteRC& source, std::vector<std::byte> payload, std::function<void(quic::message)> func);
        void handle_fetch_bootstrap_rcs(quic::message);

        // Inner handlers for relayed requests
        void _handle_path_control(quic::message, std::optional<std::string> = std::nullopt);
        void _handle_publish_cc(quic::message, std::optional<std::string> = std::nullopt);
        void _handle_fetch_rcs(quic::message, std::optional<std::string> = std::nullopt);
        void _handle_find_cc(quic::message, std::optional<std::string> = std::nullopt);
        void _handle_resolve_sns(quic::message, std::optional<std::string> = std::nullopt);
        void _handle_initiate_session(quic::message, std::optional<std::string> = std::nullopt);
        void _handle_close_session(quic::message, std::optional<std::string> = std::nullopt);
        void _handle_path_switch(quic::message, std::optional<std::string> = std::nullopt);
        void _handle_path_ping(quic::message, std::optional<std::string> = std::nullopt);

        // Path messages
        void handle_path_build(quic::message, const RouterID& from);
        void handle_path_latency(quic::message);

        // Sessions
        void handle_initiate_session(quic::message);
        void handle_close_session(quic::message);
        void handle_path_switch(quic::message);

        // These requests come over a path (as a "path_control" request),
        // we may or may not need to make a request to another relay,
        // then respond (onioned) back along the path.
        static std::unordered_map<std::string_view, void (LinkManager::*)(quic::message, std::optional<std::string>)>
            path_requests;

        // Path relaying
        void handle_path_data_message(std::vector<std::byte> msg);
        void handle_path_control(quic::message);
        void handle_path_request(quic::message, std::span<const std::byte> payload);
        void handle_session_data(std::vector<std::byte>&& payload, const session_tag& tag, const SymmNonce& nonce);

        // Path responses
        void handle_path_latency_response(quic::message);
    };
}  // namespace llarp
