#pragma once

#include <service/auth.hpp>
#include <lokimq/lokimq.h>

namespace llarp::service
{
  struct Endpoint;
}

namespace llarp::rpc
{
  struct EndpointAuthRPC : public llarp::service::IAuthPolicy,
                           public std::enable_shared_from_this<EndpointAuthRPC>
  {
    using LMQ_ptr = std::shared_ptr<lokimq::LokiMQ>;
    using Endpoint_ptr = std::shared_ptr<llarp::service::Endpoint>;
    using Whitelist_t = std::unordered_set<llarp::service::Address, llarp::service::Address::Hash>;

    explicit EndpointAuthRPC(
        std::string url,
        std::string method,
        Whitelist_t whitelist,
        LMQ_ptr lmq,
        Endpoint_ptr endpoint);
    virtual ~EndpointAuthRPC() = default;

    void
    Start();

    void
    AuthenticateAsync(
        llarp::service::Address from,
        service::ConvoTag tag,
        std::function<void(service::AuthResult)> hook) override;

   private:
    const std::string m_AuthURL;
    const std::string m_AuthMethod;
    const Whitelist_t m_AuthWhitelist;
    LMQ_ptr m_LMQ;
    Endpoint_ptr m_Endpoint;
    std::optional<lokimq::ConnectionID> m_Conn;
  };
}  // namespace llarp::rpc
