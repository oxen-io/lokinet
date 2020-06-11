#pragma once

#include <router_id.hpp>

#include <lokimq/lokimq.h>
#include <lokimq/address.h>
#include <crypto/types.hpp>

#include <crypto/types.hpp>

namespace llarp
{
  struct AbstractRouter;

  namespace rpc
  {
    using LMQ_ptr = std::shared_ptr<lokimq::LokiMQ>;

    /// The LokidRpcClient uses loki-mq to talk to make API requests to lokid.
    struct LokidRpcClient : public std::enable_shared_from_this<LokidRpcClient>
    {
      explicit LokidRpcClient(LMQ_ptr lmq, AbstractRouter* r);

      /// Connect to lokid async
      void
      ConnectAsync(lokimq::address url);

      /// blocking request identity key from lokid
      std::optional<SecretKey>
      ObtainIdentityKey();

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

      std::optional<lokimq::ConnectionID> m_Connection;
      LMQ_ptr m_lokiMQ;
      std::string m_CurrentBlockHash;

      AbstractRouter* const m_Router;
    };

  }  // namespace rpc
}  // namespace llarp
