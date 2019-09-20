#include <service/sendcontext.hpp>

#include <router/abstractrouter.hpp>
#include <routing/path_transfer_message.hpp>
#include <service/endpoint.hpp>
#include <util/thread/logic.hpp>
#include <utility>

namespace llarp
{
  namespace service
  {
    SendContext::SendContext(ServiceInfo ident, const Introduction& intro,
                             path::PathSet* send, Endpoint* ep)
        : remoteIdent(std::move(ident))
        , remoteIntro(intro)
        , m_PathSet(send)
        , m_DataHandler(ep)
        , m_Endpoint(ep)
    {
      createdAt = ep->Now();
    }

    bool
    SendContext::Send(const ProtocolFrame& msg, path::Path_ptr path)
    {
      util::Lock lock(&m_SendQueueMutex);
      m_SendQueue.emplace_back(
          std::make_shared< const routing::PathTransferMessage >(
              msg, remoteIntro.pathID),
          path);
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
          m_Endpoint->MarkConvoTagActive(item.first->T.T);
        }
        else
          LogError(m_Endpoint->Name(), " failed to send frame on path");
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

      auto path = m_PathSet->GetNewestPathByRouter(remoteIntro.router);
      if(!path)
      {
        LogError(m_Endpoint->Name(),
                 " cannot encrypt and send: no path for intro ", remoteIntro);
        return;
      }

      if(!m_DataHandler->GetCachedSessionKeyFor(f.T, shared))
      {
        LogError(m_Endpoint->Name(),
                 " has no cached session key on session T=", f.T);
        return;
      }

      ProtocolMessage m;
      m_DataHandler->PutIntroFor(f.T, remoteIntro);
      m_DataHandler->PutReplyIntroFor(f.T, path->intro);
      m.proto      = t;
      m.seqno      = m_Endpoint->GetSeqNoForConvo(f.T);
      m.introReply = path->intro;
      f.F          = m.introReply.pathID;
      m.sender     = m_Endpoint->GetIdentity().pub;
      m.tag        = f.T;
      m.PutBuffer(payload);
      if(!f.EncryptAndSign(m, shared, m_Endpoint->GetIdentity()))
      {
        LogError(m_Endpoint->Name(), " failed to sign message");
        return;
      }
      Send(f, path);
    }

    void
    SendContext::AsyncEncryptAndSendTo(const llarp_buffer_t& data,
                                       ProtocolType protocol)
    {
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
