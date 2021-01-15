#pragma once
#include <service/auth.hpp>
#include <lokimq/lokimq.h>

namespace llarp::rpc
{
  struct EndpointAuthInformer : public llarp::service::IAuthInformer
  {
    explicit EndpointAuthInformer(std::shared_ptr<lokimq::LokiMQ> lmq);

    void
    InformAuthResult(llarp::service::Address remote, llarp::service::AuthResult result) override;
  };
}  // namespace llarp::rpc
