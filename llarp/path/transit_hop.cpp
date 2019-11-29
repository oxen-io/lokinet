#include <path/path.hpp>

#include <dht/context.hpp>
#include <exit/context.hpp>
#include <exit/exit_messages.hpp>
#include <link/i_link_manager.hpp>
#include <messages/discard.hpp>
#include <messages/relay_commit.hpp>
#include <messages/relay_status.hpp>
#include <path/path_context.hpp>
#include <path/transit_hop.hpp>
#include <router/abstractrouter.hpp>
#include <routing/path_latency_message.hpp>
#include <routing/path_transfer_message.hpp>
#include <routing/handler.hpp>
#include <util/buffer.hpp>
#include <util/endian.hpp>

namespace llarp
{
  namespace path
  {
    std::ostream&
    TransitHopInfo::print(std::ostream& stream, int level, int spaces) const
    {
      Printer printer(stream, level, spaces);
      printer.printAttribute("tx", txID);
      printer.printAttribute("rx", rxID);
      printer.printAttribute("upstream", upstream);
      printer.printAttribute("downstream", downstream);

      return stream;
    }

    TransitHop::TransitHop() : m_UpstreamGather(128), m_DownstreamGather(128)
    {
      m_UpstreamGather.enable();
      m_DownstreamGather.enable();
      m_UpstreamWorkCounter   = 0;
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
    TransitHop::HandleLRSM(uint64_t status,
                           std::array< EncryptedFrame, 8 >& frames,
                           AbstractRouter* r)
    {
      auto msg    = std::make_shared< LR_StatusMessage >(frames);
      msg->status = status;
      msg->pathid = info.rxID;

      // TODO: add to IHopHandler some notion of "path status"

      const uint64_t ourStatus = LR_StatusRecord::SUCCESS;
      if(!msg->AddFrame(pathKey, ourStatus))
      {
        return false;
      }

      LR_StatusMessage::QueueSendMessage(r, info.downstream, msg);

      if((status & LR_StatusRecord::SUCCESS) == 0)
      {
        LogDebug(
            "TransitHop received non-successful LR_StatusMessage, queueing "
            "self-destruct");
        QueueDestroySelf(r);
      }

      return true;
    }

    TransitHopInfo::TransitHopInfo(const RouterID& down,
                                   const LR_CommitRecord& record)
        : txID(record.txid)
        , rxID(record.rxid)
        , upstream(record.nextHop)
        , downstream(down)
    {
    }

    bool
    TransitHop::SendRoutingMessage(const routing::IMessage& msg,
                                   AbstractRouter* r)
    {
      if(!IsEndpoint(r->pubkey()))
        return false;

      std::array< byte_t, MAX_LINK_MSG_SIZE - 128 > tmp;
      llarp_buffer_t buf(tmp);
      if(!msg.BEncode(&buf))
      {
        llarp::LogError("failed to encode routing message");
        return false;
      }
      TunnelNonce N;
      N.Randomize();
      buf.sz = buf.cur - buf.base;
      // pad to nearest MESSAGE_PAD_SIZE bytes
      auto dlt = buf.sz % pad_size;
      if(dlt)
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
    TransitHop::DownstreamWork(TrafficQueue_ptr msgs, AbstractRouter* r)
    {
      m_DownstreamWorkCounter++;
      auto flushIt = [self = shared_from_this(), r]() {
        std::vector< RelayDownstreamMessage > msgs;
        do
        {
          auto maybe = self->m_DownstreamGather.tryPopFront();
          if(not maybe.has_value())
            break;
          msgs.emplace_back(maybe.value());
        } while(true);
        self->HandleAllDownstream(std::move(msgs), r);
      };
      for(auto& ev : *msgs)
      {
        RelayDownstreamMessage msg;
        const llarp_buffer_t buf(ev.first);
        msg.pathid = info.rxID;
        msg.Y      = ev.second ^ nonceXOR;
        CryptoManager::instance()->xchacha20(buf, pathKey, ev.second);
        msg.X = buf;
        llarp::LogDebug("relay ", msg.X.size(), " bytes downstream from ",
                        info.upstream, " to ", info.downstream);
        if(m_DownstreamGather.full())
        {
          LogicCall(r->logic(), flushIt);
        }
        if(not m_DownstreamGather.disabled())
          m_DownstreamGather.pushBack(msg);
      }
      m_DownstreamWorkCounter--;
      if(m_DownstreamWorkCounter == 0)
        LogicCall(r->logic(), flushIt);
    }

    void
    TransitHop::UpstreamWork(TrafficQueue_ptr msgs, AbstractRouter* r)
    {
      m_UpstreamWorkCounter++;
      auto flushIt = [self = shared_from_this(), r]() {
        std::vector< RelayUpstreamMessage > msgs;
        do
        {
          auto maybe = self->m_UpstreamGather.tryPopFront();
          if(not maybe.has_value())
            break;
          msgs.emplace_back(maybe.value());
        } while(true);
        self->HandleAllUpstream(std::move(msgs), r);
      };
      for(auto& ev : *msgs)
      {
        const llarp_buffer_t buf(ev.first);
        RelayUpstreamMessage msg;
        CryptoManager::instance()->xchacha20(buf, pathKey, ev.second);
        msg.pathid = info.txID;
        msg.Y      = ev.second ^ nonceXOR;
        msg.X      = buf;
        if(m_UpstreamGather.full())
        {
          LogicCall(r->logic(), flushIt);
        }
        if(not m_UpstreamGather.disabled())
          m_UpstreamGather.pushBack(msg);
      }
      m_UpstreamWorkCounter--;
      if(m_UpstreamWorkCounter == 0)
        LogicCall(r->logic(), flushIt);
    }

    void
    TransitHop::HandleAllUpstream(std::vector< RelayUpstreamMessage > msgs,
                                  AbstractRouter* r)
    {
      if(IsEndpoint(r->pubkey()))
      {
        for(const auto& msg : msgs)
        {
          const llarp_buffer_t buf(msg.X);
          if(!r->ParseRoutingMessageBuffer(buf, this, info.rxID))
          {
            LogWarn("invalid upstream data on endpoint ", info);
          }
          m_LastActivity = r->Now();
        }
        FlushDownstream(r);
        for(const auto& other : m_FlushOthers)
        {
          other->FlushUpstream(r);
        }
        m_FlushOthers.clear();
      }
      else
      {
        for(const auto& msg : msgs)
        {
          llarp::LogDebug("relay ", msg.X.size(), " bytes upstream from ",
                          info.downstream, " to ", info.upstream);
          r->SendToOrQueue(info.upstream, &msg);
        }
      }
      r->PumpLL();
    }

    void
    TransitHop::HandleAllDownstream(std::vector< RelayDownstreamMessage > msgs,
                                    AbstractRouter* r)
    {
      for(const auto& msg : msgs)
      {
        llarp::LogDebug("relay ", msg.X.size(), " bytes downstream from ",
                        info.upstream, " to ", info.downstream);
        r->SendToOrQueue(info.downstream, &msg);
      }
      r->PumpLL();
    }

    void
    TransitHop::FlushUpstream(AbstractRouter* r)
    {
      if(m_UpstreamQueue && !m_UpstreamQueue->empty())
        r->threadpool()->addJob(std::bind(&TransitHop::UpstreamWork,
                                          shared_from_this(),
                                          std::move(m_UpstreamQueue), r));

      m_UpstreamQueue = nullptr;
    }

    void
    TransitHop::FlushDownstream(AbstractRouter* r)
    {
      if(m_DownstreamQueue && !m_DownstreamQueue->empty())
        r->threadpool()->addJob(std::bind(&TransitHop::DownstreamWork,
                                          shared_from_this(),
                                          std::move(m_DownstreamQueue), r));
      m_DownstreamQueue = nullptr;
    }

    bool
    TransitHop::HandleDHTMessage(const llarp::dht::IMessage& msg,
                                 AbstractRouter* r)
    {
      return r->dht()->impl->RelayRequestForPath(info.rxID, msg);
    }

    bool
    TransitHop::HandlePathLatencyMessage(
        const llarp::routing::PathLatencyMessage& msg, AbstractRouter* r)
    {
      llarp::routing::PathLatencyMessage reply;
      reply.L = msg.T;
      return SendRoutingMessage(reply, r);
    }

    bool
    TransitHop::HandlePathConfirmMessage(
        __attribute__((unused)) const llarp::routing::PathConfirmMessage& msg,
        __attribute__((unused)) AbstractRouter* r)
    {
      llarp::LogWarn("unwarranted path confirm message on ", info);
      return false;
    }

    bool
    TransitHop::HandleDataDiscardMessage(
        __attribute__((unused)) const llarp::routing::DataDiscardMessage& msg,
        __attribute__((unused)) AbstractRouter* r)
    {
      llarp::LogWarn("unwarranted path data discard message on ", info);
      return false;
    }

    bool
    TransitHop::HandleObtainExitMessage(
        const llarp::routing::ObtainExitMessage& msg, AbstractRouter* r)
    {
      if(msg.Verify()
         && r->exitContext().ObtainNewExit(msg.I, info.rxID, msg.E != 0))
      {
        llarp::routing::GrantExitMessage grant;
        grant.S = NextSeqNo();
        grant.T = msg.T;
        if(!grant.Sign(r->identity()))
        {
          llarp::LogError("Failed to sign grant exit message");
          return false;
        }
        return SendRoutingMessage(grant, r);
      }
      // TODO: exponential backoff
      // TODO: rejected policies
      llarp::routing::RejectExitMessage reject;
      reject.S = NextSeqNo();
      reject.T = msg.T;
      if(!reject.Sign(r->identity()))
      {
        llarp::LogError("Failed to sign reject exit message");
        return false;
      }
      return SendRoutingMessage(reject, r);
    }

    bool
    TransitHop::HandleCloseExitMessage(
        const llarp::routing::CloseExitMessage& msg, AbstractRouter* r)
    {
      const llarp::routing::DataDiscardMessage discard(info.rxID, msg.S);
      auto ep = r->exitContext().FindEndpointForPath(info.rxID);
      if(ep && msg.Verify(ep->PubKey()))
      {
        llarp::routing::CloseExitMessage reply;
        reply.Y = msg.Y;
        reply.S = NextSeqNo();
        if(reply.Sign(r->identity()))
        {
          if(SendRoutingMessage(reply, r))
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
      auto ep = r->exitContext().FindEndpointForPath(msg.P);
      if(ep)
      {
        if(!msg.Verify(ep->PubKey()))
          return false;

        if(ep->UpdateLocalPath(info.rxID))
        {
          llarp::routing::UpdateExitVerifyMessage reply;
          reply.T = msg.T;
          reply.S = NextSeqNo();
          return SendRoutingMessage(reply, r);
        }
      }
      // on fail tell message was discarded
      llarp::routing::DataDiscardMessage discard(info.rxID, msg.S);
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
      auto endpoint = r->exitContext().FindEndpointForPath(info.rxID);
      if(endpoint)
      {
        bool sent = true;
        for(const auto& pkt : msg.X)
        {
          // check short packet buffer
          if(pkt.size() <= 8)
            continue;
          uint64_t counter = bufbe64toh(pkt.data());
          sent &= endpoint->QueueOutboundTraffic(
              ManagedBuffer(llarp_buffer_t(pkt.data() + 8, pkt.size() - 8)),
              counter);
        }
        return sent;
      }

      llarp::LogError("No exit endpoint on ", info);
      // discarded
      llarp::routing::DataDiscardMessage discard(info.rxID, msg.S);
      return SendRoutingMessage(discard, r);
    }

    bool
    TransitHop::HandlePathTransferMessage(
        const llarp::routing::PathTransferMessage& msg, AbstractRouter* r)
    {
      auto path = r->pathContext().GetPathForTransfer(msg.P);
      llarp::routing::DataDiscardMessage discarded(msg.P, msg.S);
      if(path == nullptr || msg.T.F != info.txID)
      {
        return SendRoutingMessage(discarded, r);
      }

      std::array< byte_t, service::MAX_PROTOCOL_MESSAGE_SIZE > tmp;
      llarp_buffer_t buf(tmp);
      if(!msg.T.BEncode(&buf))
      {
        llarp::LogWarn(info, " failed to transfer data message, encode failed");
        return SendRoutingMessage(discarded, r);
      }
      // rewind
      buf.sz  = buf.cur - buf.base;
      buf.cur = buf.base;
      // send
      if(path->HandleDownstream(buf, msg.Y, r))
      {
        m_FlushOthers.emplace(path);
        return true;
      }
      return SendRoutingMessage(discarded, r);
    }

    std::ostream&
    TransitHop::print(std::ostream& stream, int level, int spaces) const
    {
      Printer printer(stream, level, spaces);
      printer.printAttribute("TransitHop", info);
      printer.printAttribute("started", started);
      printer.printAttribute("lifetime", lifetime);
      return stream;
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
      auto func = std::bind(&TransitHop::SetSelfDestruct, shared_from_this());
      LogicCall(r->logic(), func);
    }
  }  // namespace path
}  // namespace llarp
