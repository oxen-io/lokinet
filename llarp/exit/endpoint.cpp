#include "endpoint.hpp"

#include <llarp/handlers/exit.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/quic/tunnel.hpp>

namespace llarp
{
  namespace exit
  {
    Endpoint::Endpoint(
        const llarp::PubKey& remoteIdent,
        const llarp::path::HopHandler_ptr& beginPath,
        bool rewriteIP,
        huint128_t ip,
        llarp::handlers::ExitEndpoint* parent)
        : createdAt{parent->Now()}
        , m_Parent{parent}
        , m_remoteSignKey{remoteIdent}
        , m_CurrentPath{beginPath}
        , m_IP{ip}
        , m_RewriteSource{rewriteIP}
    {
      m_LastActive = parent->Now();
    }

    Endpoint::~Endpoint()
    {
      if (m_CurrentPath)
        m_Parent->DelEndpointInfo(m_CurrentPath->RXID());
    }

    void
    Endpoint::Close()
    {
      m_Parent->RemoveExit(this);
    }

    util::StatusObject
    Endpoint::ExtractStatus() const
    {
      auto now = m_Parent->Now();
      util::StatusObject obj{
          {"identity", m_remoteSignKey.ToString()},
          {"ip", m_IP.ToString()},
          {"txRate", m_TxRate},
          {"rxRate", m_RxRate},
          {"createdAt", to_json(createdAt)},
          {"exiting", !m_RewriteSource},
          {"looksDead", LooksDead(now)},
          {"expiresSoon", ExpiresSoon(now)},
          {"expired", IsExpired(now)}};
      return obj;
    }

    bool
    Endpoint::UpdateLocalPath(const llarp::PathID_t& nextPath)
    {
      if (!m_Parent->UpdateEndpointPath(m_remoteSignKey, nextPath))
        return false;
      const RouterID us{m_Parent->GetRouter()->pubkey()};
      m_CurrentPath = m_Parent->GetRouter()->pathContext().GetByUpstream(us, nextPath);
      return true;
    }

    void
    Endpoint::Tick(llarp_time_t now)
    {
      (void)now;
      m_RxRate = 0;
      m_TxRate = 0;
    }

    bool
    Endpoint::IsExpired(llarp_time_t now) const
    {
      auto path = GetCurrentPath();
      if (path)
      {
        return path->Expired(now);
      }
      // if we don't have an underlying path we are considered expired
      return true;
    }

    bool
    Endpoint::ExpiresSoon(llarp_time_t now, llarp_time_t dlt) const
    {
      if (m_CurrentPath)
        return m_CurrentPath->ExpiresSoon(now, dlt);
      return true;
    }

    bool
    Endpoint::LooksDead(llarp_time_t now, llarp_time_t timeout) const
    {
      if (ExpiresSoon(now, timeout))
        return true;
      auto path = GetCurrentPath();
      if (not path)
        return true;
      auto lastPing = path->LastRemoteActivityAt();
      if (lastPing == 0s || (now > lastPing && now - lastPing > timeout))
        return now > m_LastActive && now - m_LastActive > timeout;
      else if (lastPing > 0s)  // NOLINT
        return now > lastPing && now - lastPing > timeout;
      return lastPing > 0s;
    }

    bool
    Endpoint::QueueOutboundTraffic(
        PathID_t path, ManagedBuffer buf, uint64_t counter, service::ProtocolType t)
    {
      const service::ConvoTag tag{path.as_array()};
      if (t == service::ProtocolType::QUIC)
      {
        auto quic = m_Parent->GetQUICTunnel();
        if (not quic)
          return false;
        quic->receive_packet(tag, buf.underlying);
        m_LastActive = m_Parent->Now();
        m_TxRate += buf.underlying.sz;
        return true;
      }
      // queue overflow
      if (m_UpstreamQueue.size() > MaxUpstreamQueueSize)
        return false;

      llarp::net::IPPacket pkt;
      if (!pkt.Load(buf.underlying))
        return false;
      if (pkt.IsV6() && m_Parent->SupportsV6())
      {
        huint128_t dst;
        if (m_RewriteSource)
          dst = m_Parent->GetIfAddr();
        else
          dst = pkt.dstv6();
        pkt.UpdateIPv6Address(m_IP, dst);
      }
      else if (pkt.IsV4() && !m_Parent->SupportsV6())
      {
        huint32_t dst;
        if (m_RewriteSource)
          dst = net::TruncateV6(m_Parent->GetIfAddr());
        else
          dst = pkt.dstv4();
        pkt.UpdateIPv4Address(xhtonl(net::TruncateV6(m_IP)), xhtonl(dst));
      }
      else
      {
        return false;
      }
      m_UpstreamQueue.emplace(pkt, counter);
      m_TxRate += buf.underlying.sz;
      m_LastActive = m_Parent->Now();
      return true;
    }

    bool
    Endpoint::QueueInboundTraffic(ManagedBuffer buf, service::ProtocolType type)
    {
      llarp::net::IPPacket pkt{};
      if (type == service::ProtocolType::QUIC)
      {
        pkt.sz = std::min(buf.underlying.sz, sizeof(pkt.buf));
        std::copy_n(buf.underlying.base, pkt.sz, pkt.buf);
      }
      else
      {
        if (!pkt.Load(buf.underlying))
          return false;

        huint128_t src;
        if (m_RewriteSource)
          src = m_Parent->GetIfAddr();
        else
          src = pkt.srcv6();
        if (pkt.IsV6())
          pkt.UpdateIPv6Address(src, m_IP);
        else
          pkt.UpdateIPv4Address(xhtonl(net::TruncateV6(src)), xhtonl(net::TruncateV6(m_IP)));
      }
      const auto _pktbuf = pkt.ConstBuffer();
      auto& pktbuf = _pktbuf.underlying;

      const uint8_t queue_idx = pktbuf.sz / llarp::routing::ExitPadSize;
      if (m_DownstreamQueues.find(queue_idx) == m_DownstreamQueues.end())
        m_DownstreamQueues.emplace(queue_idx, InboundTrafficQueue_t{});
      auto& queue = m_DownstreamQueues.at(queue_idx);
      if (queue.size() == 0)
      {
        queue.emplace_back();
        queue.back().protocol = type;
        return queue.back().PutBuffer(pktbuf, m_Counter++);
      }
      auto& msg = queue.back();
      if (msg.Size() + pktbuf.sz > llarp::routing::ExitPadSize)
      {
        queue.emplace_back();
        queue.back().protocol = type;
        return queue.back().PutBuffer(pktbuf, m_Counter++);
      }
      msg.protocol = type;
      return msg.PutBuffer(pktbuf, m_Counter++);
    }

    bool
    Endpoint::Flush()
    {
      // flush upstream queue
      while (m_UpstreamQueue.size())
      {
        m_Parent->QueueOutboundTraffic(m_UpstreamQueue.top().pkt);
        m_UpstreamQueue.pop();
      }
      // flush downstream queue
      auto path = GetCurrentPath();
      bool sent = path != nullptr;
      if (path)
      {
        for (auto& item : m_DownstreamQueues)
        {
          auto& queue = item.second;
          while (queue.size())
          {
            auto& msg = queue.front();
            msg.S = path->NextSeqNo();
            if (path->SendRoutingMessage(msg, m_Parent->GetRouter()))
            {
              m_RxRate += msg.Size();
              sent = true;
            }
            queue.pop_front();
          }
        }
      }
      for (auto& item : m_DownstreamQueues)
        item.second.clear();
      return sent;
    }
  }  // namespace exit
}  // namespace llarp
