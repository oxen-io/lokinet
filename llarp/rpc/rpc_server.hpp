#pragma once

#include <string_view>
#include <oxenmq/oxenmq.h>
#include <oxenmq/address.h>

namespace llarp
{
  struct AbstractRouter;
}

namespace lokimq = oxenmq;

namespace llarp::rpc
{
  using LMQ_ptr = std::shared_ptr<lokimq::OxenMQ>;

  struct RpcServer
  {
    explicit RpcServer(LMQ_ptr, AbstractRouter*);
    ~RpcServer() = default;
    void
    AsyncServeRPC(lokimq::address addr);

   private:
    LMQ_ptr m_LMQ;
    AbstractRouter* const m_Router;
  };
}  // namespace llarp::rpc
