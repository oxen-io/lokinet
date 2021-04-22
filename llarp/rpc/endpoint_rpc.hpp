#pragma once

#include <llarp/service/auth.hpp>
#include <oxenmq/oxenmq.h>

namespace llarp::service
{
  struct Endpoint;
}

namespace llarp::rpc
{
  struct EndpointAuthRPC : public llarp::service::IAuthPolicy,
                           public std::enable_shared_from_this<EndpointAuthRPC>
  {
    using LMQ_ptr = std::shared_ptr<oxenmq::OxenMQ>;
    using Endpoint_ptr = std::shared_ptr<llarp::service::Endpoint>;
    using Whitelist_t = std::unordered_set<llarp::service::Address>;

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
        std::shared_ptr<llarp::service::ProtocolMessage> msg,
        std::function<void(service::AuthResult)> hook) override;

    bool
    AsyncAuthPending(service::ConvoTag tag) const override;

   private:
    const std::string m_AuthURL;
    const std::string m_AuthMethod;
    const Whitelist_t m_AuthWhitelist;
    LMQ_ptr m_LMQ;
    Endpoint_ptr m_Endpoint;
    std::optional<oxenmq::ConnectionID> m_Conn;
    std::unordered_set<service::ConvoTag> m_PendingAuths;
  };
}  // namespace llarp::rpc
