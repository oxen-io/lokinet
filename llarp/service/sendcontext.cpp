#include <service/sendcontext.hpp>

#include <messages/path_transfer.hpp>
#include <service/endpoint.hpp>
#include <router/abstractrouter.hpp>

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
    SendContext::Send(const ProtocolFrame& msg)
    {
      auto path = m_PathSet->GetByEndpointWithID(remoteIntro.router, msg.F);
      if(path)
      {
        const routing::PathTransferMessage transfer(msg, remoteIntro.pathID);
        if(path->SendRoutingMessage(transfer, m_Endpoint->Router()))
        {
          llarp::LogInfo("sent intro to ", remoteIntro.pathID, " on ",
                         remoteIntro.router, " seqno=", sequenceNo);
          lastGoodSend = m_Endpoint->Now();
          ++sequenceNo;
          return true;
        }
        else
          llarp::LogError("Failed to send frame on path");
      }
      else
        llarp::LogError("cannot send because we have no path to ",
                        remoteIntro.router);
      return false;
    }

    /// send on an established convo tag
    void
    SendContext::EncryptAndSendTo(const llarp_buffer_t& payload, ProtocolType t)
    {
      auto crypto = m_Endpoint->Router()->crypto();
      SharedSecret shared;
      routing::PathTransferMessage msg;
      ProtocolFrame& f = msg.T;
      f.N.Randomize();
      f.T = currentConvoTag;
      f.S = m_Endpoint->GetSeqNoForConvo(f.T);

      auto now = m_Endpoint->Now();
      if(remoteIntro.ExpiresSoon(now))
      {
        // shift intro
        if(MarkCurrentIntroBad(now))
        {
          llarp::LogInfo("intro shifted");
        }
      }
      auto path = m_PathSet->GetNewestPathByRouter(remoteIntro.router);
      if(!path)
      {
        llarp::LogError("cannot encrypt and send: no path for intro ",
                        remoteIntro);
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
          llarp::LogError("failed to sign");
          return;
        }
      }
      else
      {
        llarp::LogError("No cached session key");
        return;
      }

      msg.P = remoteIntro.pathID;
      msg.Y.Randomize();
      if(path->SendRoutingMessage(msg, m_Endpoint->Router()))
      {
        llarp::LogDebug("sent message via ", remoteIntro.pathID, " on ",
                        remoteIntro.router);
        ++sequenceNo;
        lastGoodSend = now;
      }
      else
      {
        llarp::LogWarn("Failed to send routing message for data");
      }
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
          llarp::LogWarn("no good path yet, your message may drop");
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
