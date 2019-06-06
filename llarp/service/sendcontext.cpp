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
      {
        util::Lock lock(&m_SendQueueMutex);
        m_SendQueue.emplace_back(transfer, path);
      }
      return true;
    }

    void
    SendContext::FlushUpstream()
    {
      auto r = m_Endpoint->Router();
      util::Lock lock(&m_SendQueueMutex);
      for(const auto& item : m_SendQueue)
      {
        if(item.second->SendRoutingMessage(*item.first, r))
        {
          lastGoodSend = r->Now();
        }
        else
          LogError("Failed to send frame on path");
      }
      m_SendQueue.clear();
    }

    /// send on an established convo tag
    void
    SendContext::EncryptAndSendTo(const llarp_buffer_t& payload, ProtocolType t)
    {
      SharedSecret shared;
      ProtocolFrame f;
      f.N.Randomize();
      f.T = currentConvoTag;
      f.S = ++sequenceNo;

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

      if(!m_DataHandler->GetCachedSessionKeyFor(f.T, shared))
      {
        LogError("No cached session key");
        return;
      }

      ProtocolMessage m;
      m_DataHandler->PutIntroFor(f.T, remoteIntro);
      m_DataHandler->PutReplyIntroFor(f.T, path->intro);
      m.proto      = t;
      m.seqno      = m_Endpoint->GetSeqNoForConvo(f.T);
      m.introReply = path->intro;
      f.F          = m.introReply.pathID;
      f.S          = 0;
      m.sender     = m_Endpoint->GetIdentity().pub;
      m.tag        = f.T;
      m.PutBuffer(payload);
      if(!f.EncryptAndSign(m, shared, m_Endpoint->GetIdentity()))
      {
        LogError("failed to sign");
        return;
      }
      Send(f, path);
    }

    void
    SendContext::AsyncEncryptAndSendTo(const llarp_buffer_t& data,
                                       ProtocolType protocol)
    {
      auto now = m_Endpoint->Now();
      if(remoteIntro.ExpiresSoon(now))
      {
        if(!ShiftIntroduction())
        {
          LogWarn("no good path yet, your message may drop");
        }
      }
      if(lastGoodSend)
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
