
#include "transit_hop.hpp"
#include "path_context.hpp"

#include <llarp/dht/context.hpp>
#include <llarp/exit/exit_messages.hpp>
#include <llarp/link/i_link_manager.hpp>
#include <llarp/messages/discard.hpp>
#include <llarp/messages/relay_commit.hpp>
#include <llarp/messages/relay_status.hpp>
#include <llarp/routing/transfer_traffic_message.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/routing/path_latency_message.hpp>
#include <llarp/routing/path_transfer_message.hpp>
#include <llarp/routing/handler.hpp>

#include <llarp/util/buffer.hpp>

#include <oxenc/endian.h>

namespace llarp
{
  namespace path
  {

    static auto logcat = log::Cat("transit-hop");

    std::string
    TransitHopInfo::ToString() const
    {
      return fmt::format(
          "[TransitHopInfo tx={} rx={} upstream={} downstream={}]",
          txID,
          rxID,
          upstream,
          downstream);
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
    TransitHop::HandleLRSM(
        uint64_t status, std::array<EncryptedFrame, 8>& frames, AbstractRouter* r)
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

    bool
    TransitHop::SendRoutingMessage(const routing::IMessage& msg, AbstractRouter* r)
    {
      if (!IsEndpoint(r->pubkey()))
        return false;

      std::array<byte_t, MAX_LINK_MSG_SIZE - 128> tmp;
      llarp_buffer_t buf(tmp);
      if (!msg.BEncode(&buf))
      {
        llarp::LogError("failed to encode routing message");
        return false;
      }
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
        msg.Y = ev.second ^ nonceXOR;
        CryptoManager::instance()->xchacha20(buf, pathKey, ev.second);
        msg.X = buf;
        llarp::LogDebug(
            "relay ",
            msg.X.size(),
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
        msg.Y = ev.second ^ nonceXOR;
        msg.X = buf;
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
          const llarp_buffer_t buf(msg.X);
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
              msg.X.size(),
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
            msg.X.size(),
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
    TransitHop::HandleDHTMessage(const llarp::dht::IMessage& msg, AbstractRouter* r)
    {
      return r->dht()->impl->RelayRequestForPath(info.rxID, msg);
    }

    bool
    TransitHop::HandlePathLatencyMessage(
        const llarp::routing::PathLatencyMessage& msg, AbstractRouter* r)
    {
      llarp::routing::PathLatencyMessage reply;
      reply.L = msg.T;
      reply.S = msg.S;
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
      if (not msg.Verify())
      {
        // invalid sig, tell them their request was discarded.
        return SendRoutingMessage(routing::DataDiscardMessage{info.rxID, msg.S}, r);
      }
      // this message is always asking for our local snode.
      // future protocol versions are expected to permit non local routing destinations.
      RouterID to{r->pubkey()};
      auto destination = r->get_layers()->route->create_destination(info, msg.source_identity, to);
      if (not destination)
      {
        // we did not give the requester an allocation.
        // send them a reject.
        llarp::routing::RejectExitMessage reject;
        reject.S = NextSeqNo();
        reject.txid = msg.txid;
        // todo: exponential backoff
        reject.backoff = 2000;
        if (not reject.Sign(r->identity()))
        {
          log::info(
              logcat,
              "snode destionation for {} to {} was rejected by us"_format(msg.source_identity, to));
          return false;
        }
      }
      llarp::routing::GrantExitMessage grant;
      grant.S = NextSeqNo();
      grant.txid = msg.txid;
      if (not grant.Sign(r->identity()))
      {
        // failed to sign, tell them it was discarded after removing routing destination.
        r->get_layers()->route->remove_destination_on(info);
        return SendRoutingMessage(routing::DataDiscardMessage{info.rxID, msg.S}, r);
      }
      return SendRoutingMessage(grant, r);
    }

    bool
    TransitHop::HandleCloseExitMessage(
        const llarp::routing::CloseExitMessage& msg, AbstractRouter* r)
    {
      const llarp::routing::DataDiscardMessage discard{info.rxID, msg.S};

      auto maybe_dest = r->get_layers()->route->destination_on(info);
      if (not maybe_dest)
        return SendRoutingMessage(discard, r);

      if (not msg.Verify(maybe_dest->src))
        return SendRoutingMessage(discard, r);

      llarp::routing::CloseExitMessage reply;
      reply.Y = msg.Y;
      reply.S = NextSeqNo();

      r->get_layers()->route->remove_destination_on(info);

      reply.Sign(r->identity());
      return SendRoutingMessage(reply, r);
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
      auto maybe_previous_dest = r->get_layers()->route->destination_from(msg.path_id);
      // make sure we have the last destination.
      if (not maybe_previous_dest)
        return SendRoutingMessage(routing::DataDiscardMessage{info.rxID, msg.S}, r);
      // verify that the update message was signed by the previous mapping's identity.
      if (not msg.Verify(maybe_previous_dest->src))
        return SendRoutingMessage(routing::DataDiscardMessage{info.rxID, msg.S}, r);

      auto next_dest = r->get_layers()->route->create_destination(
          info, maybe_previous_dest->src, maybe_previous_dest->dst);
      // allocate new routing destination.
      if (not next_dest)
        return SendRoutingMessage(routing::DataDiscardMessage{info.rxID, msg.S}, r);

      // remove old routing destination.
      // r->get_layers()->route->remove_destination_on(maybe_previous_dest->onion_info);
      llarp::routing::UpdateExitVerifyMessage reply;
      reply.T = msg.T;
      reply.S = NextSeqNo();
      return SendRoutingMessage(reply, r);
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
    TransitHop::HandleGrantExitMessage(
        const llarp::routing::GrantExitMessage& msg, AbstractRouter* r)
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
      auto maybe_dest = r->get_layers()->route->destination_on(info);
      // no destination on this path.
      if (not maybe_dest)
        return SendRoutingMessage(llarp::routing::DataDiscardMessage{info.rxID, msg.S}, r);
      // destination not applicable for this use case.
      if (not maybe_dest->flow_info)
        return SendRoutingMessage(llarp::routing::DataDiscardMessage{info.rxID, msg.S}, r);

      const auto& flow_info = *maybe_dest->flow_info;

      // propagate all traffic up to flow layer.
      for (auto& traffic : msg.to_flow_traffic())
      {
        traffic.flow_info = flow_info;
        r->get_layers()->flow->offer_flow_traffic(std::move(traffic));
      }
      return true;
    }

    bool
    TransitHop::HandlePathTransferMessage(
        const llarp::routing::PathTransferMessage& msg, AbstractRouter* r)
    {
      auto path = r->pathContext().GetPathForTransfer(msg.P);
      llarp::routing::DataDiscardMessage discarded{msg.P, msg.S};
      if (path == nullptr || msg.T.F != info.txID)
      {
        return SendRoutingMessage(discarded, r);
      }
      // send routing message
      if (path->SendRoutingMessage(msg.T, r))
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
  }  // namespace path
}  // namespace llarp
