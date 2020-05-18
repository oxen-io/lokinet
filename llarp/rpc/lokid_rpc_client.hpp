#pragma once

#include <router_id.hpp>

#include <lokimq/lokimq.h>

#include <future>

namespace llarp
{
  namespace rpc
  {
    /// The LokidRpcClient uses loki-mq to talk to make API requests to lokid.
    struct LokidRpcClient
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
      /// TODO: take lokid pubkey and other auth parameters
      LokidRpcClient(std::string lokidPubkey);

      /// Connect to lokid
      void
      connect();

      /// Initiates a ping request to lokid, currently used to let lokid know that lokinet is still
      /// running (required to prevent a Service Node from being deregistered).
      ///
      /// This uses the "lokinet_ping" API endpoint.
      std::future<void>
      ping();

      /// Requests the most recent known block hash from lokid
      ///
      /// This uses the "poll_block_hash" API endpoint.
      std::future<std::string>
      requestNextBlockHash();

      /// Requests a full list of known service nodes from lokid
      ///
      /// This uses the "get_n_service_nodes" API endpoint.
      std::future<std::vector<RouterID>>
      requestServiceNodeList();

     private:
      std::string m_lokidPubkey;
      lokimq::ConnectionID m_lokidConnectionId;
      lokimq::LokiMQ m_lokiMQ;

      void
      request();
    };

  }  // namespace rpc
}  // namespace llarp
