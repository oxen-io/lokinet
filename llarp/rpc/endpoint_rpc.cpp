#include "endpoint_rpc.hpp"
#include <service/endpoint.hpp>

namespace llarp::rpc
{
  EndpointAuthRPC::EndpointAuthRPC(
      std::string url,
      std::string method,
      Whitelist_t whitelist,
      LMQ_ptr lmq,
      Endpoint_ptr endpoint)
      : m_AuthURL(std::move(url))
      , m_AuthMethod(std::move(method))
      , m_AuthWhitelist(std::move(whitelist))
      , m_LMQ(std::move(lmq))
      , m_Endpoint(std::move(endpoint))
  {
  }

  void
  EndpointAuthRPC::Start()
  {
    if (m_AuthURL.empty())
      return;
    m_LMQ->connect_remote(
        m_AuthURL,
        [self = shared_from_this()](lokimq::ConnectionID c) {
          self->m_Conn = std::move(c);
          LogInfo("connected to endpoint auth server via ", *self->m_Conn);
        },
        [self = shared_from_this()](lokimq::ConnectionID, std::string_view fail) {
          LogWarn("failed to connect to endpoint auth server: ", fail);
          self->m_Endpoint->RouterLogic()->call_later(1s, [self]() { self->Start(); });
        });
  }

  void
  EndpointAuthRPC::AuthenticateAsync(
      llarp::service::Address from,
      llarp::service::ConvoTag,
      std::function<void(service::AuthResult)> hook)
  {
    if (m_AuthWhitelist.count(from))
    {
      m_Endpoint->RouterLogic()->Call([hook]() { hook(service::AuthResult::eAuthAccepted); });
      return;
    }
    if (not m_Conn.has_value())
    {
      m_Endpoint->RouterLogic()->Call([hook]() { hook(service::AuthResult::eAuthFailed); });
      return;
    }
    // call method with 1 parameter: the loki address of the remote
    m_LMQ->request(
        *m_Conn,
        m_AuthMethod,
        [self = shared_from_this(), hook, from = from.ToString()](
            bool success, std::vector<std::string> data) {
          service::AuthResult result = service::AuthResult::eAuthFailed;
          if (success and not data.empty())
          {
            const auto maybe = service::ParseAuthResult(data[0]);
            if (maybe.has_value())
            {
              result = *maybe;
            }
          }
          self->m_Endpoint->RouterLogic()->Call([hook, result]() { hook(result); });
        },
        from.ToString());
  }  // namespace llarp::rpc

}  // namespace llarp::rpc
