#pragma once

#include <llarp/crypto/types.hpp>
#include <llarp/dht/key.hpp>
#include <llarp/router_id.hpp>
#include <llarp/service/name.hpp>

#include <oxenmq/address.h>
#include <oxenmq/oxenmq.h>

namespace llarp
{
    struct Router;

    namespace rpc
    {
        /// The LokidRpcClient uses loki-mq to talk to make API requests to lokid.
        struct LokidRpcClient : public std::enable_shared_from_this<LokidRpcClient>
        {
            explicit LokidRpcClient(std::shared_ptr<oxenmq::OxenMQ> lmq, std::weak_ptr<Router> r);

            /// Connect to lokid async
            void ConnectAsync(oxenmq::address url);

            /// blocking request identity key from lokid
            /// throws on failure
            SecretKey ObtainIdentityKey();

            /// get what the current block height is according to oxend
            uint64_t BlockHeight() const
            {
                return m_BlockHeight;
            }

            void lookup_ons_hash(
                std::string namehash,
                std::function<void(std::optional<service::EncryptedName>)> resultHandler);

            /// inform that if connected to a router successfully
            void InformConnection(RouterID router, bool success);

            void StartPings();

           private:
            /// do a lmq command on the current connection
            void Command(std::string_view cmd);

            /// triggers a service node list refresh from oxend; thread-safe and will do nothing if
            /// an update is already in progress.
            void UpdateServiceNodeList();

            template <typename HandlerFunc_t, typename Args_t>
            void Request(std::string_view cmd, HandlerFunc_t func, const Args_t& args)
            {
                m_lokiMQ->request(*m_Connection, std::move(cmd), std::move(func), args);
            }

            template <typename HandlerFunc_t>
            void Request(std::string_view cmd, HandlerFunc_t func)
            {
                m_lokiMQ->request(*m_Connection, std::move(cmd), std::move(func));
            }

            // Handles a service node list update; takes the "service_node_states" object of an
            // oxend "get_service_nodes" rpc request.
            void HandleNewServiceNodeList(const nlohmann::json& json);

            // Handles notification of a new block
            void HandleNewBlock(oxenmq::Message& msg);

            std::optional<oxenmq::ConnectionID> m_Connection;
            std::shared_ptr<oxenmq::OxenMQ> m_lokiMQ;

            std::weak_ptr<Router> m_Router;
            std::atomic<bool> m_UpdatingList;
            std::string m_LastUpdateHash;

            std::unordered_map<RouterID, PubKey> m_KeyMap;

            uint64_t m_BlockHeight;
        };

    }  // namespace rpc
}  // namespace llarp
