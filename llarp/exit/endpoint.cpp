#include <exit/endpoint.hpp>

#include <handlers/exit.hpp>
#include <path/path_context.hpp>
#include <router/abstractrouter.hpp>

namespace llarp
{
  namespace exit
  {
    Endpoint::Endpoint(
        const llarp::PubKey& remoteIdent,
        const llarp::PathID_t& beginPath,
        bool rewriteIP,
        huint128_t ip,
        llarp::handlers::ExitEndpoint* parent)
        : createdAt(parent->Now())
        , m_Parent(parent)
        , m_remoteSignKey(remoteIdent)
        , m_CurrentPath(beginPath)
        , m_IP(ip)
        , m_RewriteSource(rewriteIP)
        , m_Counter(0)
    {
      m_LastActive = parent->Now();
    }

    Endpoint::~Endpoint()
    {
      m_Parent->DelEndpointInfo(m_CurrentPath);
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
      util::StatusObject obj{{"identity", m_remoteSignKey.ToString()},
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
      m_CurrentPath = nextPath;
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
      auto path = GetCurrentPath();
      if (path)
        return path->ExpiresSoon(now, dlt);
      return true;
    }

    bool
    Endpoint::LooksDead(llarp_time_t now, llarp_time_t timeout) const
    {
      if (ExpiresSoon(now, timeout))
        return true;
      auto path = GetCurrentPath();
      if (!path)
        return true;
      auto lastPing = path->LastRemoteActivityAt();
      if (lastPing == 0s || (now > lastPing && now - lastPing > timeout))
        return now > m_LastActive && now - m_LastActive > timeout;
      else if (lastPing > 0s)  // NOLINT
        return now > lastPing && now - lastPing > timeout;
      return lastPing > 0s;
    }

    bool
    Endpoint::QueueOutboundTraffic(ManagedBuffer buf, uint64_t counter)
    {
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
    Endpoint::QueueInboundTraffic(ManagedBuffer buf)
    {
      llarp::net::IPPacket pkt;
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

      const auto _pktbuf = pkt.Buffer();
      const llarp_buffer_t& pktbuf = _pktbuf.underlying;
      const uint8_t queue_idx = pktbuf.sz / llarp::routing::ExitPadSize;
      if (m_DownstreamQueues.find(queue_idx) == m_DownstreamQueues.end())
        m_DownstreamQueues.emplace(queue_idx, InboundTrafficQueue_t{});
      auto& queue = m_DownstreamQueues.at(queue_idx);
      if (queue.size() == 0)
      {
        queue.emplace_back();
        return queue.back().PutBuffer(pktbuf, m_Counter++);
      }
      auto& msg = queue.back();
      if (msg.Size() + pktbuf.sz > llarp::routing::ExitPadSize)
      {
        queue.emplace_back();
        return queue.back().PutBuffer(pktbuf, m_Counter++);
      }

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

    llarp::path::HopHandler_ptr
    Endpoint::GetCurrentPath() const
    {
      auto router = m_Parent->GetRouter();
      return router->pathContext().GetByUpstream(router->pubkey(), m_CurrentPath);
    }
  }  // namespace exit
}  // namespace llarp
