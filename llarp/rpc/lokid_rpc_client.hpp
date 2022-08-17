#pragma once

#include <llarp/router_id.hpp>

#include <oxenmq/oxenmq.h>
#include <oxenmq/address.h>
#include <llarp/crypto/types.hpp>
#include <llarp/dht/key.hpp>
#include <llarp/service/name.hpp>

namespace llarp
{
  struct AbstractRouter;

  namespace rpc
  {
    using LMQ_ptr = std::shared_ptr<oxenmq::OxenMQ>;

    /// The LokidRpcClient uses loki-mq to talk to make API requests to lokid.
    struct LokidRpcClient : public std::enable_shared_from_this<LokidRpcClient>
    {
      explicit LokidRpcClient(LMQ_ptr lmq, std::weak_ptr<AbstractRouter> r);

      /// Connect to lokid async
      void
      ConnectAsync(oxenmq::address url);

      /// blocking request identity key from lokid
      /// throws on failure
      SecretKey
      ObtainIdentityKey();

      /// get what the current block height is according to oxend
      uint64_t
      BlockHeight() const
      {
        return m_BlockHeight;
      }

      void
      LookupLNSNameHash(
          dht::Key_t namehash,
          std::function<void(std::optional<service::EncryptedName>)> resultHandler);

      /// inform that if connected to a router successfully
      void
      InformConnection(RouterID router, bool success);

     private:
      /// called when we have connected to lokid via lokimq
      void
      Connected();

      /// do a lmq command on the current connection
      void
      Command(std::string_view cmd);

      void
      UpdateServiceNodeList();

      void
      UpdateSelfInfo();

      template <typename HandlerFunc_t, typename Args_t>
      void
      Request(std::string_view cmd, HandlerFunc_t func, const Args_t& args)
      {
        m_lokiMQ->request(*m_Connection, std::move(cmd), std::move(func), args);
      }

      template <typename HandlerFunc_t>
      void
      Request(std::string_view cmd, HandlerFunc_t func)
      {
        m_lokiMQ->request(*m_Connection, std::move(cmd), std::move(func));
      }

      void
      HandleGotServiceNodeList(std::string json);

      // Handles request from lokid for peer stats on a specific peer
      void
      HandleGetPeerStats(oxenmq::Message& msg);

      // Handles notification of a new block
      void
      HandleNewBlock(oxenmq::Message& msg);

      std::optional<oxenmq::ConnectionID> m_Connection;
      LMQ_ptr m_lokiMQ;

      std::weak_ptr<AbstractRouter> m_Router;
      std::atomic<bool> m_UpdatingList;
      std::atomic<bool> m_UpdatingSelfInfo;

      std::unordered_map<RouterID, PubKey> m_KeyMap;

      uint64_t m_BlockHeight;
    };

  }  // namespace rpc
}  // namespace llarp
