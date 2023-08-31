#include "path.hpp"

#include <llarp/dht/context.hpp>
#include <llarp/exit/context.hpp>
#include <llarp/exit/exit_messages.hpp>
#include <llarp/link/i_link_manager.hpp>
#include <llarp/messages/discard.hpp>
#include <llarp/messages/relay_commit.hpp>
#include <llarp/messages/relay_status.hpp>
#include "path_context.hpp"
#include "transit_hop.hpp"
#include <llarp/router/abstractrouter.hpp>
#include <llarp/routing/path_latency_message.hpp>
#include <llarp/routing/path_transfer_message.hpp>
#include <llarp/routing/handler.hpp>
#include <llarp/util/buffer.hpp>

#include <oxenc/endian.h>

namespace llarp::path
{
  std::string
  TransitHopInfo::ToString() const
  {
    return fmt::format(
        "[TransitHopInfo tx={} rx={} upstream={} downstream={}]", txID, rxID, upstream, downstream);
  }

  TransitHop::TransitHop()
      : IHopHandler{}
      , m_UpstreamGather{transit_hop_queue_size}
      , m_DownstreamGather{transit_hop_queue_size}
  {
    m_UpstreamGather.enable();
    m_DownstreamGather.enable();
    m_UpstreamWorkCounter = 0;
    m_DownstreamWorkCounter = 0;
  }

  bool
  TransitHop::Expired(llarp_time_t now) const
  {
    return destroy || (now >= ExpireTime());
  }

  llarp_time_t
  TransitHop::ExpireTime() const
  {
    return started + lifetime;
  }

  bool
  TransitHop::HandleLRSM(uint64_t status, std::array<EncryptedFrame, 8>& frames, AbstractRouter* r)
  {
    auto msg = std::make_shared<LR_StatusMessage>(frames);
    msg->status = status;
    msg->pathid = info.rxID;

    // TODO: add to IHopHandler some notion of "path status"

    const uint64_t ourStatus = LR_StatusRecord::SUCCESS;

    msg->AddFrame(pathKey, ourStatus);
    LR_StatusMessage::QueueSendMessage(r, info.downstream, msg, shared_from_this());
    return true;
  }

  TransitHopInfo::TransitHopInfo(const RouterID& down, const LR_CommitRecord& record)
      : txID(record.txid), rxID(record.rxid), upstream(record.nextHop), downstream(down)
  {}

  /** Note: this is one of two places where AbstractRoutingMessage::bt_encode() is called, the
      other of which is llarp/path/path.cpp in Path::SendRoutingMessage(). For now,
      we will default to the override of ::bt_encode() that returns an std::string. The role that
      llarp_buffer_t plays here is likely superfluous, and can be replaced with either a leaner
      llarp_buffer, or just handled using strings.

      One important consideration is the frequency at which routing messages are sent, making
      superfluous copies important to optimize out here. We have to instantiate at least one
      std::string whether we pass a bt_dict_producer as a reference or create one within the
      ::bt_encode() call.

      If we decide to stay with std::strings, the function Path::HandleUpstream (along with the
      functions it calls and so on) will need to be modified to take an std::string that we can
      std::move around.
  */
  bool
  TransitHop::SendRoutingMessage(const routing::AbstractRoutingMessage& msg, AbstractRouter* r)
  {
    if (!IsEndpoint(r->pubkey()))
      return false;

    std::array<byte_t, MAX_LINK_MSG_SIZE - 128> tmp;
    llarp_buffer_t buf(tmp);

    auto bte = msg.bt_encode();
    buf.write(bte.begin(), bte.end());

    TunnelNonce N;
    N.Randomize();
    buf.sz = buf.cur - buf.base;
    // pad to nearest MESSAGE_PAD_SIZE bytes
    auto dlt = buf.sz % pad_size;
    if (dlt)
    {
      dlt = pad_size - dlt;
      // randomize padding
      CryptoManager::instance()->randbytes(buf.cur, dlt);
      buf.sz += dlt;
    }
    buf.cur = buf.base;
    return HandleDownstream(buf, N, r);
  }

  void
  TransitHop::DownstreamWork(TrafficQueue_t msgs, AbstractRouter* r)
  {
    auto flushIt = [self = shared_from_this(), r]() {
      std::vector<RelayDownstreamMessage> msgs;
      while (auto maybe = self->m_DownstreamGather.tryPopFront())
      {
        msgs.push_back(*maybe);
      }
      self->HandleAllDownstream(std::move(msgs), r);
    };
    for (auto& ev : msgs)
    {
      RelayDownstreamMessage msg;
      const llarp_buffer_t buf(ev.first);
      msg.pathid = info.rxID;
      msg.nonce = ev.second ^ nonceXOR;
      CryptoManager::instance()->xchacha20(buf, pathKey, ev.second);
      msg.enc = buf;
      llarp::LogDebug(
          "relay ",
          msg.enc.size(),
          " bytes downstream from ",
          info.upstream,
          " to ",
          info.downstream);
      if (m_DownstreamGather.full())
      {
        r->loop()->call(flushIt);
      }
      if (m_DownstreamGather.enabled())
        m_DownstreamGather.pushBack(msg);
    }
    r->loop()->call(flushIt);
  }

  void
  TransitHop::UpstreamWork(TrafficQueue_t msgs, AbstractRouter* r)
  {
    for (auto& ev : msgs)
    {
      const llarp_buffer_t buf(ev.first);
      RelayUpstreamMessage msg;
      CryptoManager::instance()->xchacha20(buf, pathKey, ev.second);
      msg.pathid = info.txID;
      msg.nonce = ev.second ^ nonceXOR;
      msg.enc = buf;
      if (m_UpstreamGather.tryPushBack(msg) != thread::QueueReturn::Success)
        break;
    }

    // Flush it:
    r->loop()->call([self = shared_from_this(), r] {
      std::vector<RelayUpstreamMessage> msgs;
      while (auto maybe = self->m_UpstreamGather.tryPopFront())
      {
        msgs.push_back(*maybe);
      }
      self->HandleAllUpstream(std::move(msgs), r);
    });
  }

  void
  TransitHop::HandleAllUpstream(std::vector<RelayUpstreamMessage> msgs, AbstractRouter* r)
  {
    if (IsEndpoint(r->pubkey()))
    {
      for (const auto& msg : msgs)
      {
        const llarp_buffer_t buf(msg.enc);
        if (!r->ParseRoutingMessageBuffer(buf, this, info.rxID))
        {
          LogWarn("invalid upstream data on endpoint ", info);
        }
        m_LastActivity = r->Now();
      }
      FlushDownstream(r);
      for (const auto& other : m_FlushOthers)
      {
        other->FlushDownstream(r);
      }
      m_FlushOthers.clear();
    }
    else
    {
      for (const auto& msg : msgs)
      {
        llarp::LogDebug(
            "relay ",
            msg.enc.size(),
            " bytes upstream from ",
            info.downstream,
            " to ",
            info.upstream);
        r->SendToOrQueue(info.upstream, msg);
      }
    }
    r->TriggerPump();
  }

  void
  TransitHop::HandleAllDownstream(std::vector<RelayDownstreamMessage> msgs, AbstractRouter* r)
  {
    for (const auto& msg : msgs)
    {
      llarp::LogDebug(
          "relay ",
          msg.enc.size(),
          " bytes downstream from ",
          info.upstream,
          " to ",
          info.downstream);
      r->SendToOrQueue(info.downstream, msg);
    }
    r->TriggerPump();
  }

  void
  TransitHop::FlushUpstream(AbstractRouter* r)
  {
    if (not m_UpstreamQueue.empty())
    {
      r->QueueWork([self = shared_from_this(),
                    data = std::exchange(m_UpstreamQueue, {}),
                    r]() mutable { self->UpstreamWork(std::move(data), r); });
    }
  }

  void
  TransitHop::FlushDownstream(AbstractRouter* r)
  {
    if (not m_DownstreamQueue.empty())
    {
      r->QueueWork([self = shared_from_this(),
                    data = std::exchange(m_DownstreamQueue, {}),
                    r]() mutable { self->DownstreamWork(std::move(data), r); });
    }
  }

  /// this is where a DHT message is handled at the end of a path, that is,
  /// where a SNode receives a DHT message from a client along a path.
  bool
  TransitHop::HandleDHTMessage(const llarp::dht::AbstractDHTMessage& msg, AbstractRouter* r)
  {
    return r->dht()->impl->RelayRequestForPath(info.rxID, msg);
  }

  bool
  TransitHop::HandlePathLatencyMessage(
      const llarp::routing::PathLatencyMessage& msg, AbstractRouter* r)
  {
    llarp::routing::PathLatencyMessage reply;
    reply.latency = msg.sent_time;
    reply.sequence_number = msg.sequence_number;
    return SendRoutingMessage(reply, r);
  }

  bool
  TransitHop::HandlePathConfirmMessage(
      [[maybe_unused]] const llarp::routing::PathConfirmMessage& msg,
      [[maybe_unused]] AbstractRouter* r)
  {
    llarp::LogWarn("unwarranted path confirm message on ", info);
    return false;
  }

  bool
  TransitHop::HandleDataDiscardMessage(
      [[maybe_unused]] const llarp::routing::DataDiscardMessage& msg,
      [[maybe_unused]] AbstractRouter* r)
  {
    llarp::LogWarn("unwarranted path data discard message on ", info);
    return false;
  }

  bool
  TransitHop::HandleObtainExitMessage(
      const llarp::routing::ObtainExitMessage& msg, AbstractRouter* r)
  {
    if (msg.Verify() && r->exitContext().ObtainNewExit(msg.pubkey, info.rxID, msg.flag != 0))
    {
      llarp::routing::GrantExitMessage grant;
      grant.sequence_number = NextSeqNo();
      grant.tx_id = msg.tx_id;
      if (!grant.Sign(r->identity()))
      {
        llarp::LogError("Failed to sign grant exit message");
        return false;
      }
      return SendRoutingMessage(grant, r);
    }
    // TODO: exponential backoff
    // TODO: rejected policies
    llarp::routing::RejectExitMessage reject;
    reject.sequence_number = NextSeqNo();
    reject.tx_id = msg.tx_id;
    if (!reject.Sign(r->identity()))
    {
      llarp::LogError("Failed to sign reject exit message");
      return false;
    }
    return SendRoutingMessage(reject, r);
  }

  bool
  TransitHop::HandleCloseExitMessage(const llarp::routing::CloseExitMessage& msg, AbstractRouter* r)
  {
    const llarp::routing::DataDiscardMessage discard(info.rxID, msg.sequence_number);
    auto ep = r->exitContext().FindEndpointForPath(info.rxID);
    if (ep && msg.Verify(ep->PubKey()))
    {
      llarp::routing::CloseExitMessage reply;
      reply.nonce = msg.nonce;
      reply.sequence_number = NextSeqNo();
      if (reply.Sign(r->identity()))
      {
        if (SendRoutingMessage(reply, r))
        {
          ep->Close();
          return true;
        }
      }
    }
    return SendRoutingMessage(discard, r);
  }

  bool
  TransitHop::HandleUpdateExitVerifyMessage(
      const llarp::routing::UpdateExitVerifyMessage& msg, AbstractRouter* r)
  {
    (void)msg;
    (void)r;
    llarp::LogError("unwarranted exit verify on ", info);
    return false;
  }

  bool
  TransitHop::HandleUpdateExitMessage(
      const llarp::routing::UpdateExitMessage& msg, AbstractRouter* r)
  {
    auto ep = r->exitContext().FindEndpointForPath(msg.path_id);
    if (ep)
    {
      if (!msg.Verify(ep->PubKey()))
        return false;

      if (ep->UpdateLocalPath(info.rxID))
      {
        llarp::routing::UpdateExitVerifyMessage reply;
        reply.tx_id = msg.tx_id;
        reply.sequence_number = NextSeqNo();
        return SendRoutingMessage(reply, r);
      }
    }
    // on fail tell message was discarded
    llarp::routing::DataDiscardMessage discard(info.rxID, msg.sequence_number);
    return SendRoutingMessage(discard, r);
  }

  bool
  TransitHop::HandleRejectExitMessage(
      const llarp::routing::RejectExitMessage& msg, AbstractRouter* r)
  {
    (void)msg;
    (void)r;
    llarp::LogError(info, " got unwarranted RXM");
    return false;
  }

  bool
  TransitHop::HandleGrantExitMessage(const llarp::routing::GrantExitMessage& msg, AbstractRouter* r)
  {
    (void)msg;
    (void)r;
    llarp::LogError(info, " got unwarranted GXM");
    return false;
  }

  bool
  TransitHop::HandleTransferTrafficMessage(
      const llarp::routing::TransferTrafficMessage& msg, AbstractRouter* r)
  {
    auto endpoint = r->exitContext().FindEndpointForPath(info.rxID);
    if (endpoint)
    {
      bool sent = true;
      for (const auto& pkt : msg.enc_buf)
      {
        // check short packet buffer
        if (pkt.size() <= 8)
          continue;
        auto counter = oxenc::load_big_to_host<uint64_t>(pkt.data());
        llarp_buffer_t buf{pkt.data() + 8, pkt.size() - 8};
        sent =
            endpoint->QueueOutboundTraffic(info.rxID, buf.copy(), counter, msg.protocol) and sent;
      }
      return sent;
    }

    llarp::LogError("No exit endpoint on ", info);
    // discarded
    llarp::routing::DataDiscardMessage discard(info.rxID, msg.sequence_number);
    return SendRoutingMessage(discard, r);
  }

  bool
  TransitHop::HandlePathTransferMessage(
      const llarp::routing::PathTransferMessage& msg, AbstractRouter* r)
  {
    auto path = r->pathContext().GetPathForTransfer(msg.path_id);
    llarp::routing::DataDiscardMessage discarded{msg.path_id, msg.sequence_number};
    if (path == nullptr || msg.protocol_frame_msg.path_id != info.txID)
    {
      return SendRoutingMessage(discarded, r);
    }
    // send routing message
    if (path->SendRoutingMessage(msg.protocol_frame_msg, r))
    {
      m_FlushOthers.emplace(path);
      return true;
    }
    return SendRoutingMessage(discarded, r);
  }

  std::string
  TransitHop::ToString() const
  {
    return fmt::format(
        "[TransitHop {} started={} lifetime={}", info, started.count(), lifetime.count());
  }

  void
  TransitHop::Stop()
  {
    m_UpstreamGather.disable();
    m_DownstreamGather.disable();
  }

  void
  TransitHop::SetSelfDestruct()
  {
    destroy = true;
  }

  void
  TransitHop::QueueDestroySelf(AbstractRouter* r)
  {
    r->loop()->call([self = shared_from_this()] { self->SetSelfDestruct(); });
  }
}  // namespace llarp::path
