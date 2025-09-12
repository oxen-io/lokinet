#pragma once

#include <llarp/contact/router_id.hpp>
#include <llarp/contact/sns.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/util/logging.hpp>

#include <oxenmq/address.h>
#include <oxenmq/oxenmq.h>

namespace oxen::quic
{
    struct Ticker;
}
namespace llarp
{
    class Router;
}

namespace llarp::rpc
{
    inline constexpr auto PING_INTERVAL{30s};

    /// OxenMQ RPC client for a relay to talk to its oxend to obtain info about and report on
    /// the service node network.
    class OxendRPC
    {
      public:
        OxendRPC(oxenmq::OxenMQ& omq, Router& r);

        /// Connect to lokid async
        void connect_async(oxenmq::address url);

        /// blocking request identity key from lokid
        /// throws on failure
        Ed25519SecretKey obtain_identity_key();

        /// get what the current block height is according to oxend
        uint64_t block_height() const { return _block_height; }

        void lookup_sns_hash(
            std::string namehash, std::function<void(std::optional<EncryptedSNSRecord>)> resultHandler);

        /// inform that if connected to a router successfully
        void inform_connection(RouterID router, bool success);

        void start_pings();

        /// triggers a service node list refresh from oxend; thread-safe and will do nothing if
        /// an update is already in progress.  The promise is for router.cpp to attempt a
        /// synchronous update on startup, and should not be used otherwise.
        void update_service_node_list(std::shared_ptr<std::promise<void>> on_update = nullptr);

      private:
        void ping();

        /// do a lmq command on the current connection
        void command(std::string_view cmd);

        template <typename HandlerFunc_t, typename Args_t>
        void request(std::string_view cmd, HandlerFunc_t func, const Args_t& args)
        {
            _omq.request(*_conn, std::move(cmd), std::move(func), args);
        }

        template <typename HandlerFunc_t>
        void request(std::string_view cmd, HandlerFunc_t func)
        {
            _omq.request(*_conn, std::move(cmd), std::move(func));
        }

        // Handles a service node list update; takes the "service_node_states" object of an
        // oxend "get_service_nodes" rpc request.
        void handle_new_service_node_list(const nlohmann::json& json);

        // Handles notification of a new block
        void handle_new_block(oxenmq::Message& msg);

        std::shared_ptr<oxen::quic::Ticker> _ping_ticker;

        std::optional<oxenmq::ConnectionID> _conn;
        oxenmq::OxenMQ& _omq;

        Router& _router;
        std::atomic<bool> _is_updating_list;
        std::string _last_hash_update;

        std::unordered_map<RouterID, PubKey> _key_map;

        uint64_t _block_height;
    };

}  // namespace llarp::rpc
