#pragma once

#include <string_view>
#include <llarp/config/config.hpp>
#include <oxenmq/oxenmq.h>
#include <oxenmq/address.h>
#include <oxen/log/omq_logger.hpp>

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
    AddRPCCats();

   private:
    void
    HandleLogsSubRequest(oxenmq::Message& m);

    LMQ_ptr m_LMQ;
    AbstractRouter* const m_Router;

    oxen::log::PubsubLogger log_subs;
  };
}  // namespace llarp::rpc
