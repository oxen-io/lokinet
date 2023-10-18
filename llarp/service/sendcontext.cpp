#include "sendcontext.hpp"

#include <llarp/path/path.hpp>
#include <llarp/router/router.hpp>
#include <llarp/routing/path_transfer_message.hpp>
#include "endpoint.hpp"
#include <utility>
#include <unordered_set>
#include <llarp/crypto/crypto.hpp>

namespace llarp::service
{
  static constexpr size_t SendContextQueueSize = 512;

  SendContext::SendContext(
      ServiceInfo ident, const Introduction& intro, path::PathSet* send, Endpoint* ep)
      : remoteIdent(std::move(ident))
      , remoteIntro(intro)
      , m_PathSet(send)
      , service_endpoint(ep)
      , createdAt(ep->Now())
  {}

  bool
  SendContext::Send(std::shared_ptr<ProtocolFrameMessage> msg, path::Path_ptr path)
  {
    if (path->IsReady()
        and m_SendQueue.tryPushBack(std::make_pair(
                std::make_shared<routing::PathTransferMessage>(*msg, remoteIntro.path_id), path))
            == thread::QueueReturn::Success)
    {
      service_endpoint->router()->TriggerPump();
      return true;
    }
    return false;
  }

  void
  SendContext::FlushUpstream()
  {
    auto r = service_endpoint->router();
    std::unordered_set<path::Path_ptr, path::Ptr_Hash> flushpaths;
    auto rttRMS = 0ms;
    while (auto maybe = m_SendQueue.tryPopFront())
    {
      auto& [msg, path] = *maybe;
      msg->sequence_number = path->NextSeqNo();
      if (path->SendRoutingMessage(*msg, r))
      {
        lastGoodSend = r->now();
        flushpaths.emplace(path);
        service_endpoint->ConvoTagTX(msg->protocol_frame_msg.convo_tag);
        const auto rtt = (path->intro.latency + remoteIntro.latency) * 2;
        rttRMS += rtt * rtt.count();
      }
    }
    // flush the select path's upstream
    for (const auto& path : flushpaths)
    {
      path->FlushUpstream(r);
    }
    if (flushpaths.empty())
      return;
    estimatedRTT = std::chrono::milliseconds{
        static_cast<int64_t>(std::sqrt(rttRMS.count() / flushpaths.size()))};
  }

  /// send on an established convo tag
  void
  SendContext::EncryptAndSendTo(const llarp_buffer_t& payload, ProtocolType t)
  {
    SharedSecret shared;
    auto f = std::make_shared<ProtocolFrameMessage>();
    f->flag = 0;
    f->nonce.Randomize();
    f->convo_tag = currentConvoTag;
    f->sequence_number = ++sequenceNo;

    auto path = m_PathSet->GetPathByRouter(remoteIntro.router);
    if (!path)
    {
      ShiftIntroRouter(remoteIntro.router);
      LogWarn(m_PathSet->Name(), " cannot encrypt and send: no path for intro ", remoteIntro);
      return;
    }

    if (!service_endpoint->GetCachedSessionKeyFor(f->convo_tag, shared))
    {
      LogWarn(
          m_PathSet->Name(),
          " could not send, has no cached session key on session T=",
          f->convo_tag);
      return;
    }

    auto m = std::make_shared<ProtocolMessage>();
    service_endpoint->PutIntroFor(f->convo_tag, remoteIntro);
    service_endpoint->PutReplyIntroFor(f->convo_tag, path->intro);
    m->proto = t;
    if (auto maybe = service_endpoint->GetSeqNoForConvo(f->convo_tag))
    {
      m->seqno = *maybe;
    }
    else
    {
      LogWarn(m_PathSet->Name(), " could not get sequence number for session T=", f->convo_tag);
      return;
    }
    m->introReply = path->intro;
    f->path_id = m->introReply.path_id;
    m->sender = service_endpoint->GetIdentity().pub;
    m->tag = f->convo_tag;
    m->put_buffer(payload);
    service_endpoint->router()->queue_work([f, m, shared, path, this] {
      if (not f->EncryptAndSign(*m, shared, service_endpoint->GetIdentity()))
      {
        LogError(m_PathSet->Name(), " failed to sign message");
        return;
      }
      Send(f, path);
    });
  }

  void
  SendContext::AsyncSendAuth(std::function<void(AuthResult)> resultHandler)
  {
    if (const auto maybe = service_endpoint->MaybeGetAuthInfoForEndpoint(remoteIdent.Addr()))
    {
      // send auth message
      const llarp_buffer_t authdata{maybe->token};
      AsyncGenIntro(authdata, ProtocolType::Auth);
      authResultListener = resultHandler;
    }
    else
      resultHandler({AuthResultCode::eAuthAccepted, "no auth needed"});
  }

  void
  SendContext::AsyncEncryptAndSendTo(const llarp_buffer_t& data, ProtocolType protocol)
  {
    if (IntroSent())
    {
      EncryptAndSendTo(data, protocol);
      return;
    }
    // have we generated the initial intro but not sent it yet? bail here so we don't cause
    // bullshittery
    if (IntroGenerated() and not IntroSent())
    {
      LogWarn(
          m_PathSet->Name(),
          " we have generated an intial handshake but have not sent it yet so we drop a packet "
          "to prevent bullshittery");
      return;
    }
    const auto maybe = service_endpoint->MaybeGetAuthInfoForEndpoint(remoteIdent.Addr());
    if (maybe.has_value())
    {
      // send auth message
      const llarp_buffer_t authdata(maybe->token);
      AsyncGenIntro(authdata, ProtocolType::Auth);
    }
    else
    {
      AsyncGenIntro(data, protocol);
    }
  }
}  // namespace llarp::service
