#include "endpoint_rpc.hpp"
#include <llarp/service/endpoint.hpp>

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
        [self = shared_from_this()](oxenmq::ConnectionID c) {
          self->m_Conn = std::move(c);
          LogInfo("connected to endpoint auth server via ", *self->m_Conn);
        },
        [self = shared_from_this()](oxenmq::ConnectionID, std::string_view fail) {
          LogWarn("failed to connect to endpoint auth server: ", fail);
          self->m_Endpoint->Loop()->call_later(1s, [self] { self->Start(); });
        });
  }

  bool
  EndpointAuthRPC::AsyncAuthPending(service::ConvoTag tag) const
  {
    return m_PendingAuths.count(tag) > 0;
  }

  void
  EndpointAuthRPC::AuthenticateAsync(
      std::shared_ptr<llarp::service::ProtocolMessage> msg,
      std::function<void(service::AuthResult)> hook)
  {
    service::ConvoTag tag = msg->tag;
    m_PendingAuths.insert(tag);
    const auto from = msg->sender.Addr();
    auto reply = m_Endpoint->Loop()->make_caller([this, tag, hook](service::AuthResult result) {
      m_PendingAuths.erase(tag);
      hook(result);
    });
    if (m_AuthWhitelist.count(from))
    {
      // explicitly whitelisted source
      reply(service::AuthResult{service::AuthResultCode::eAuthAccepted, "explicitly whitelisted"});
      return;
    }
    if (not m_Conn.has_value())
    {
      // we don't have a connection to the backend so it's failed
      reply(service::AuthResult{
          service::AuthResultCode::eAuthFailed, "remote has no connection to auth backend"});
      return;
    }

    if (msg->proto != llarp::service::ProtocolType::Auth)
    {
      // not an auth message, reject
      reply(service::AuthResult{service::AuthResultCode::eAuthRejected, "protocol error"});
      return;
    }

    const auto authinfo = msg->EncodeAuthInfo();
    std::string_view metainfo{authinfo.data(), authinfo.size()};
    std::string_view payload{(char*)msg->payload.data(), msg->payload.size()};
    // call method with 2 parameters: metainfo and userdata
    m_LMQ->request(
        *m_Conn,
        m_AuthMethod,
        [self = shared_from_this(), reply = std::move(reply)](
            bool success, std::vector<std::string> data) {
          service::AuthResult result{service::AuthResultCode::eAuthFailed, "no reason given"};
          if (success and not data.empty())
          {
            if (const auto maybe = service::ParseAuthResultCode(data[0]))
            {
              result.code = *maybe;
            }
            if (result.code == service::AuthResultCode::eAuthAccepted)
            {
              result.reason = "OK";
            }
            if (data.size() > 1)
            {
              result.reason = data[1];
            }
          }
          reply(result);
        },
        metainfo,
        payload);
  }

}  // namespace llarp::rpc
