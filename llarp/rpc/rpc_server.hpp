#pragma once

#include <string_view>
#include <oxenmq/oxenmq.h>
#include <oxenmq/address.h>

namespace llarp
{
  struct AbstractRouter;
}

namespace llarp::rpc
{
  using LMQ_ptr = std::shared_ptr<oxenmq::OxenMQ>;

  struct RpcServer
  {
    explicit RpcServer(LMQ_ptr, AbstractRouter*);
    ~RpcServer() = default;
    void
    AsyncServeRPC(oxenmq::address addr);

   private:
    LMQ_ptr m_LMQ;
    AbstractRouter* const m_Router;
  };
}  // namespace llarp::rpc
