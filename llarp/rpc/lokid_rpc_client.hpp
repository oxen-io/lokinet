#pragma once

#include <router_id.hpp>

#include <oxenmq/oxenmq.h>
#include <oxenmq/address.h>
#include <crypto/types.hpp>
#include <dht/key.hpp>
#include <service/name.hpp>

namespace lokimq = oxenmq;

namespace llarp
{
  struct AbstractRouter;

  namespace rpc
  {
    using LMQ_ptr = std::shared_ptr<lokimq::OxenMQ>;

    /// The LokidRpcClient uses loki-mq to talk to make API requests to lokid.
    struct LokidRpcClient : public std::enable_shared_from_this<LokidRpcClient>
    {
      explicit LokidRpcClient(LMQ_ptr lmq, AbstractRouter* r);

      /// Connect to lokid async
      void
      ConnectAsync(lokimq::address url);

      /// blocking request identity key from lokid
      /// throws on failure
      SecretKey
      ObtainIdentityKey();

      void
      LookupLNSNameHash(
          dht::Key_t namehash,
          std::function<void(std::optional<service::EncryptedName>)> resultHandler);

     private:
      /// called when we have connected to lokid via lokimq
      void
      Connected();

      /// do a lmq command on the current connection
      void
      Command(std::string_view cmd);

      void
      UpdateServiceNodeList();

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
      HandleGetPeerStats(lokimq::Message& msg);

      std::optional<lokimq::ConnectionID> m_Connection;
      LMQ_ptr m_lokiMQ;
      std::string m_CurrentBlockHash;

      AbstractRouter* const m_Router;
    };

  }  // namespace rpc
}  // namespace llarp
