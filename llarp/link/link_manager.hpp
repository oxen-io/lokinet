#pragma once

#include "connection.hpp"
#include "endpoint.hpp"

#include <llarp/address/address.hpp>
#include <llarp/constants/path.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/messages/common.hpp>
#include <llarp/path/transit_hop.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/compare_ptr.hpp>
#include <llarp/util/decaying_hashset.hpp>
#include <llarp/util/zstd.hpp>

#include <oxen/quic/btstream.hpp>
#include <oxen/quic/connection.hpp>
#include <oxen/quic/connection_ids.hpp>
#include <oxen/quic/endpoint.hpp>
#include <oxen/quic/format.hpp>
#include <oxen/quic/loop.hpp>
#include <oxen/quic/opt.hpp>

#include <atomic>
#include <chrono>
#include <unordered_map>

namespace llarp
{
    class Router;
}
namespace llarp::link
{
    using session::session_tag;
    // Keep-alive and idle timeouts.  For relay-to-relay connections, the keep-alive is every 10s;
    // for client-to-relay connections, we use a longer, 20s keep-alive.
    //
    // The related target idle timeout on each is set to 3 times the keep-alive, plus 3s, so that we
    // have to have no back-and-forth traffic (including the ping) for a little more than three
    // consecutive pings.
    //
    // For inbound connections, relays use the *client* idle timeout: the actual idle timeout for
    // the connection is negotiated during connection establishing, with both sides using the lower
    // value, and so client<->relay will use the longer (client) timeout, while relay<->relay will
    // use the shorter value from the outbound relay connection.
    inline constexpr auto RELAY_OUTBOUND_KEEP_ALIVE = 10s;
    inline constexpr auto RELAY_OUTBOUND_IDLE_TIMEOUT = 33s;

    inline constexpr auto CLIENT_KEEP_ALIVE = 20s;
    inline constexpr auto CLIENT_IDLE_TIMEOUT = 63s;

    inline constexpr auto RELAY_INBOUND_IDLE_TIMEOUT = std::max(CLIENT_IDLE_TIMEOUT, RELAY_OUTBOUND_IDLE_TIMEOUT);

    inline const auto RELAY_ALPN = "Lokinet_R"s;
    inline const auto CLIENT_ALPN = "Lokinet_C"s;

    // Special ALPN used when bootstrapping; unlike the above, this does not replace any existing
    // connection (e.g. if an already-connected pubkey reconnects) and these connections are not
    // used as general relay or client connections.  This ALPN only supports a single BT stream
    // command, bfetch_rcs, issued from the client to the server.
    inline const auto BOOTSTRAP_ALPN = "Lokinet_BS"s;
    inline const auto BOOTSTRAP_IDLE_TIMEOUT = 10s;

    class Manager
    {
      public:
        explicit Manager(Router& r);

        Router& router;

      private:
        friend class Endpoint;
        friend class llarp::NodeDB;

        util::DecayingHashSet<RouterID> clients{path::MAX_LIFETIME};

        quic::Address addr;

        std::atomic<bool> is_stopping{false};

        std::optional<zstd::compressor> compressor;

        // Registers commands on the client or relay end of a client-relay or relay-relay connection
        // NB: this could be called from either the network or router loop thread!
        void register_commands(quic::BTRequestStream& s, const std::variant<RouterID, quic::ConnectionID>& remote);

        // Registered the bootstrap command (bfetch_rcs) on the server (i.e. incoming) bootstrap
        // connection (i.e.  to the relay being used as a bootstrap).  The client side of such a
        // connection doesn't have any commands to register.
        // NB: this could be called from either the network or router loop thread!
        void register_bootstrap_commands(quic::BTRequestStream& s);

      public:
        link::Endpoint endpoint;

        const quic::Address& local() { return addr; }

        bool have_client_connection_to(const RouterID& remote) const;

        // TODO FIXME: see comments in router.cpp about required fixes!
        // void test_reachability(const RouterID& rid, connection_established_callback, connection_closed_callback);

        void connect_to(
            const RelayContact& rc, connection_established_callback = nullptr, connection_closed_callback = nullptr);

        // Closes all connections and releases the network event loop.
        void stop();

        nlohmann::json extract_status() const;

        // Attempts to connect to a number of random routers.
        //
        // This will try to connect to *up to* num_conns routers, but will not
        // check if we already have a connection to any of the random set, as making
        // that thread safe would be slow...I think.
        void connect_to_keep_alive(int num_conns);

        // Sends the given RC to all our relay peers, excluding connections to the RC pubkey itself,
        // and (if not-nullptr) the given quic connection.  Returns the number of relay connections
        // we sent it to.
        int gossip_rc(const RelayContact& rc, const quic::ConnectionID* sender = nullptr);

        ~Manager();

      private:
        void handle_gossip_rc(quic::message);

        void handle_fetch_bootstrap_rcs(quic::message m);

        void handle_direct_request(
            void (Manager::*respond)(std::span<const std::byte>, std::function<void(std::string)>, bool),
            quic::message m);

        // handlers for requests which could come over a path or a relay request
        void handle_publish_cc(
            std::span<const std::byte> body, std::function<void(std::string)> respond, bool source_is_relay = true);
        void handle_find_cc(
            std::span<const std::byte> body, std::function<void(std::string)> respond, bool source_is_relay = true);
        void handle_fetch_rcs(
            std::span<const std::byte> body, std::function<void(std::string)> respond, bool source_is_relay = true);

        void handle_path_control(quic::message);

        // handlers for path requests
        void handle_path_publish_cc(std::span<const std::byte> body, std::function<void(std::string)> respond);
        void handle_path_fetch_router_ids(std::span<const std::byte> body, std::function<void(std::string)> respond);
        void handle_path_find_cc(std::span<const std::byte> body, std::function<void(std::string)> respond);
        void handle_path_fetch_rcs(std::span<const std::byte> body, std::function<void(std::string)> respond);
        void handle_path_resolve_sns(std::span<const std::byte> body, std::function<void(std::string)> respond);
        void handle_path_ping(std::span<const std::byte> body, std::function<void(std::string)> respond);

        // Path messages
        void handle_path_build(quic::message, const std::variant<RouterID, quic::ConnectionID>& from);
        void handle_path_latency(quic::message);

        void handle_path_session_control(quic::message m);

        // session messages
        void handle_initiate_session(quic::message, std::optional<std::string> = std::nullopt);
        void handle_close_session(quic::message, std::optional<std::string> = std::nullopt);
        void handle_path_switch(quic::message, std::optional<std::string> = std::nullopt);

        // These requests come over a path (as a "path_control" request),
        // we may or may not need to make a request to another relay,
        // then respond (onioned) back along the path.
        static std::unordered_map<
            std::string_view,
            void (Manager::*)(std::span<const std::byte> payload, std::function<void(std::string)> respond)>
            path_requests;

        // Path relaying
        void handle_session_message(std::vector<std::byte> msg, bool control = false);
        void handle_path_request(std::span<const std::byte> payload, std::function<void(std::string)> respond);
        void handle_session_data(std::vector<std::byte>&& payload, const session_tag& tag, const SymmNonce& nonce);
        void handle_session_control(std::vector<std::byte>&& payload, const session_tag& tag, const SymmNonce& nonce);

        // Path responses
        void handle_path_latency_response(quic::message);
    };
}  // namespace llarp::link
