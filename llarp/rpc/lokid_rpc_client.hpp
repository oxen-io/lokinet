#pragma once

#include <router_id.hpp>

#include <lokimq/lokimq.h>

namespace llarp
{
  struct AbstractRouter;

  namespace rpc
  {
    using LMQ_ptr = std::shared_ptr<lokimq::LokiMQ>;

    /// The LokidRpcClient uses loki-mq to talk to make API requests to lokid.
    struct LokidRpcClient : public std::enable_shared_from_this<LokidRpcClient>
    {
      /// Not copyable or movable (because lokimq::LokiMQ is not copyable or movable).
      /// Consider wrapping in a std::unique_ptr or std::shared_ptr if you need to pass this around.
      LokidRpcClient(const LokidRpcClient&) = delete;
      LokidRpcClient&
      operator=(const LokidRpcClient&) = delete;
      LokidRpcClient(LokidRpcClient&&) = delete;
      LokidRpcClient&
      operator=(LokidRpcClient&&) = delete;

      /// Constructor
      LokidRpcClient(LMQ_ptr lmq, AbstractRouter* r);

      /// Connect to lokid async
      void
      ConnectAsync(std::string_view url);

     private:
      /// called when we have connected to lokid via lokimq
      void
      Connected();

      /// do a lmq command on the current connection
      void
      Command(std::string_view cmd);

      template <typename HandlerFunc_t, typename Args_t>
      void
      Request(std::string_view cmd, HandlerFunc_t func, const Args_t& args)
      {
        m_lokiMQ->request(*m_Connection, std::move(cmd), std::move(func), args);
      }

      void
      HandleGotServiceNodeList(std::string json);

      std::optional<lokimq::ConnectionID> m_Connection;
      LMQ_ptr m_lokiMQ;
      std::string m_CurrentBlockHash;

      AbstractRouter* const m_Router;

      void
      request();
    };

  }  // namespace rpc
}  // namespace llarp
