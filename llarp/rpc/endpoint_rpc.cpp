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
  {}

  void
  EndpointAuthRPC::Start()
  {
    if (m_AuthURL.empty() or m_AuthMethod.empty())
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
      std::shared_ptr<llarp::service::ProtocolMessage> msg,
      std::function<void(service::AuthResult)> hook)
  {
    const auto from = msg->sender.Addr();
    auto reply = [logic = m_Endpoint->RouterLogic(), hook](service::AuthResult result) {
      LogicCall(logic, [hook, result]() { hook(result); });
    };
    if (m_AuthWhitelist.count(from))
    {
      // explicitly whitelisted source
      reply(service::AuthResult::eAuthAccepted);
      return;
    }
    if (not m_Conn.has_value())
    {
      // we don't have a connection to the backend so it's failed
      reply(service::AuthResult::eAuthFailed);
      return;
    }

    if (msg->proto != llarp::service::eProtocolAuth)
    {
      // not an auth message, reject
      reply(service::AuthResult::eAuthRejected);
      return;
    }

    const auto authinfo = msg->EncodeAuthInfo();
    std::string_view metainfo{authinfo.data(), authinfo.size()};
    std::string_view payload{(char*)msg->payload.data(), msg->payload.size()};
    // call method with 2 parameters: metainfo and userdata
    m_LMQ->request(
        *m_Conn,
        m_AuthMethod,
        [self = shared_from_this(), reply](bool success, std::vector<std::string> data) {
          service::AuthResult result = service::AuthResult::eAuthFailed;
          if (success and not data.empty())
          {
            const auto maybe = service::ParseAuthResult(data[0]);
            if (maybe.has_value())
            {
              result = *maybe;
            }
          }
          reply(result);
        },
        metainfo,
        payload);
  }

}  // namespace llarp::rpc
