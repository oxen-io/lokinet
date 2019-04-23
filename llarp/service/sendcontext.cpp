#include <service/sendcontext.hpp>

#include <messages/path_transfer.hpp>
#include <service/endpoint.hpp>
#include <router/abstractrouter.hpp>
#include <util/logic.hpp>

namespace llarp
{
  namespace service
  {
    SendContext::SendContext(const ServiceInfo& ident,
                             const Introduction& intro, path::PathSet* send,
                             Endpoint* ep)
        : remoteIdent(ident)
        , remoteIntro(intro)
        , m_PathSet(send)
        , m_DataHandler(ep)
        , m_Endpoint(ep)
    {
      createdAt = ep->Now();
      currentConvoTag.Zero();
    }

    bool
    SendContext::Send(const ProtocolFrame& msg, path::Path_ptr path)
    {
      auto transfer = std::make_shared< const routing::PathTransferMessage >(
          msg, remoteIntro.pathID);
      m_Endpoint->RouterLogic()->queue_func([=]() {
        if(path->SendRoutingMessage(*transfer, m_Endpoint->Router()))
        {
          lastGoodSend = m_Endpoint->Now();
          ++sequenceNo;
        }
        else
          LogError("Failed to send frame on path");
      });
      return true;
    }

    /// send on an established convo tag
    void
    SendContext::EncryptAndSendTo(const llarp_buffer_t& payload, ProtocolType t)
    {
      auto crypto = m_Endpoint->Router()->crypto();
      SharedSecret shared;
      ProtocolFrame f;
      f.N.Randomize();
      f.T = currentConvoTag;
      f.S = m_Endpoint->GetSeqNoForConvo(f.T);

      auto now = m_Endpoint->Now();
      if(remoteIntro.ExpiresSoon(now))
      {
        // shift intro
        if(MarkCurrentIntroBad(now))
        {
          LogInfo("intro shifted");
        }
      }
      auto path = m_PathSet->GetNewestPathByRouter(remoteIntro.router);
      if(!path)
      {
        LogError("cannot encrypt and send: no path for intro ", remoteIntro);
        return;
      }

      if(m_DataHandler->GetCachedSessionKeyFor(f.T, shared))
      {
        ProtocolMessage m;
        m_DataHandler->PutIntroFor(f.T, remoteIntro);
        m_DataHandler->PutReplyIntroFor(f.T, path->intro);
        m.proto      = t;
        m.introReply = path->intro;
        f.F          = m.introReply.pathID;
        m.sender     = m_Endpoint->GetIdentity().pub;
        m.tag        = f.T;
        m.PutBuffer(payload);
        if(!f.EncryptAndSign(crypto, m, shared, m_Endpoint->GetIdentity()))
        {
          LogError("failed to sign");
          return;
        }
      }
      else
      {
        LogError("No cached session key");
        return;
      }
      ++sequenceNo;
      Send(f, path);
    }

    void
    SendContext::AsyncEncryptAndSendTo(const llarp_buffer_t& data,
                                       ProtocolType protocol)
    {
      auto now = m_Endpoint->Now();
      if(remoteIntro.ExpiresSoon(now))
      {
        if(!MarkCurrentIntroBad(now))
        {
          LogWarn("no good path yet, your message may drop");
        }
      }
      if(sequenceNo)
      {
        EncryptAndSendTo(data, protocol);
      }
      else
      {
        AsyncGenIntro(data, protocol);
      }
    }
  }  // namespace service

}  // namespace llarp
