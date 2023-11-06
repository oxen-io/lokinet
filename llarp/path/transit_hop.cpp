#include "transit_hop.hpp"

#include <llarp/router/router.hpp>
#include <llarp/util/buffer.hpp>

namespace llarp::path
{
  std::string
  TransitHopInfo::ToString() const
  {
    return fmt::format(
        "[TransitHopInfo tx={} rx={} upstream={} downstream={}]", txID, rxID, upstream, downstream);
  }

  TransitHop::TransitHop()
      : AbstractHopHandler{}
      , m_UpstreamGather{TRANSIT_HOP_QUEUE_SIZE}
      , m_DownstreamGather{TRANSIT_HOP_QUEUE_SIZE}
  {
    m_UpstreamGather.enable();
    m_DownstreamGather.enable();
    m_UpstreamWorkCounter = 0;
    m_DownstreamWorkCounter = 0;
  }

  void
  TransitHop::onion(ustring& data, TunnelNonce& nonce, bool randomize) const
  {
    if (randomize)
      nonce.Randomize();
    nonce = crypto::onion(data.data(), data.size(), pathKey, nonce, nonceXOR);
  }

  void
  TransitHop::onion(std::string& data, TunnelNonce& nonce, bool randomize) const
  {
    if (randomize)
      nonce.Randomize();
    nonce = crypto::onion(
        reinterpret_cast<unsigned char*>(data.data()), data.size(), pathKey, nonce, nonceXOR);
  }

  std::string
  TransitHop::onion_and_payload(
      std::string& payload, PathID_t next_id, std::optional<TunnelNonce> nonce) const
  {
    TunnelNonce n;
    auto& nref = nonce ? *nonce : n;
    onion(payload, nref, not nonce);

    return path::make_onion_payload(nref, next_id, payload);
  }

  bool
  TransitHop::send_path_control_message(
      std::string, std::string, std::function<void(std::string)>)
  {
    return true;
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

  TransitHopInfo::TransitHopInfo(const RouterID& down) : downstream(down)
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
  TransitHop::SendRoutingMessage(std::string payload, Router* r)
  {
    if (!IsEndpoint(r->pubkey()))
      return false;

    TunnelNonce N;
    N.Randomize();
    // pad to nearest MESSAGE_PAD_SIZE bytes
    auto dlt = payload.size() % PAD_SIZE;

    if (dlt)
    {
      dlt = PAD_SIZE - dlt;
      // randomize padding
      crypto::randbytes(reinterpret_cast<uint8_t*>(payload.data()), dlt);
    }

    // TODO: relay message along

    return true;
  }

  void
  TransitHop::DownstreamWork(TrafficQueue_t msgs, Router* r)
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

      // const llarp_buffer_t buf(ev.first);
      uint8_t* buf = ev.first.data();
      size_t sz = ev.first.size();

      msg.pathid = info.rxID;
      msg.nonce = ev.second ^ nonceXOR;

      crypto::xchacha20(buf, sz, pathKey, ev.second);
      std::memcpy(msg.enc.data(), buf, sz);

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
  TransitHop::UpstreamWork(TrafficQueue_t msgs, Router* r)
  {
    for (auto& ev : msgs)
    {
      RelayUpstreamMessage msg;

      uint8_t* buf = ev.first.data();
      size_t sz = ev.first.size();

      crypto::xchacha20(buf, sz, pathKey, ev.second);

      msg.pathid = info.txID;
      msg.nonce = ev.second ^ nonceXOR;
      std::memcpy(msg.enc.data(), buf, sz);

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
  TransitHop::HandleAllUpstream(std::vector<RelayUpstreamMessage> msgs, Router* r)
  {
    if (IsEndpoint(r->pubkey()))
    {
      for (const auto& msg : msgs)
      {
        const llarp_buffer_t buf(msg.enc);
        if (!r->ParseRoutingMessageBuffer(buf, *this, info.rxID))
        {
          LogWarn("invalid upstream data on endpoint ", info);
        }
        m_LastActivity = r->now();
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
        r->send_data_message(info.upstream, msg.bt_encode());
      }
    }
    r->TriggerPump();
  }

  void
  TransitHop::HandleAllDownstream(std::vector<RelayDownstreamMessage> msgs, Router* r)
  {
    for (const auto& msg : msgs)
    {
      log::debug(
          path_cat,
          "Relaying {} bytes downstream from {} to {}",
          msg.enc.size(),
          info.upstream,
          info.downstream);
      // TODO: is this right?
      r->send_data_message(info.downstream, msg.bt_encode());
    }

    r->TriggerPump();
  }

  void
  TransitHop::FlushUpstream(Router* r)
  {
    if (not m_UpstreamQueue.empty())
    {
      r->queue_work([self = shared_from_this(),
                     data = std::exchange(m_UpstreamQueue, {}),
                     r]() mutable { self->UpstreamWork(std::move(data), r); });
    }
  }

  void
  TransitHop::FlushDownstream(Router* r)
  {
    if (not m_DownstreamQueue.empty())
    {
      r->queue_work([self = shared_from_this(),
                     data = std::exchange(m_DownstreamQueue, {}),
                     r]() mutable { self->DownstreamWork(std::move(data), r); });
    }
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
  TransitHop::QueueDestroySelf(Router* r)
  {
    r->loop()->call([self = shared_from_this()] { self->SetSelfDestruct(); });
  }
}  // namespace llarp::path
