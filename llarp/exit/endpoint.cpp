#include <exit/endpoint.hpp>
#include <handlers/exit.hpp>
#include <router.hpp>

namespace llarp
{
  namespace exit
  {
    Endpoint::Endpoint(const llarp::PubKey& remoteIdent,
                       const llarp::PathID_t& beginPath, bool rewriteIP,
                       huint32_t ip, llarp::handlers::ExitEndpoint* parent)
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

    bool
    Endpoint::UpdateLocalPath(const llarp::PathID_t& nextPath)
    {
      if(!m_Parent->UpdateEndpointPath(m_remoteSignKey, nextPath))
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
      if(path)
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
      if(path)
        return path->ExpiresSoon(now, dlt);
      return true;
    }

    bool
    Endpoint::LooksDead(llarp_time_t now, llarp_time_t timeout) const
    {
      if(ExpiresSoon(now, timeout))
        return true;
      auto path = GetCurrentPath();
      if(!path)
        return true;
      auto lastPing = path->LastRemoteActivityAt();
      if(lastPing == 0 || ( now > lastPing && now - lastPing > timeout))
        return now > m_LastActive && now - m_LastActive > timeout;
      else if(lastPing)
        return now > lastPing && now - lastPing > timeout;
      return lastPing > 0;
    }

    bool
    Endpoint::QueueOutboundTraffic(llarp_buffer_t buf, uint64_t counter)
    {
      // queue overflow
      if(m_UpstreamQueue.size() > MaxUpstreamQueueSize)
        return false;

      llarp::net::IPv4Packet pkt;
      if(!pkt.Load(buf))
        return false;

      huint32_t dst;
      if(m_RewriteSource)
        dst = m_Parent->GetIfAddr();
      else
        dst = pkt.dst();
      pkt.UpdateIPv4PacketOnDst(m_IP, dst);
      m_UpstreamQueue.emplace(pkt, counter);
      m_TxRate += buf.sz;
      m_LastActive = m_Parent->Now();
      return true;
    }

    bool
    Endpoint::QueueInboundTraffic(llarp_buffer_t buf)
    {
      llarp::net::IPv4Packet pkt;
      if(!pkt.Load(buf))
        return false;

      huint32_t src;
      if(m_RewriteSource)
        src = m_Parent->GetIfAddr();
      else
        src = pkt.src();
      pkt.UpdateIPv4PacketOnDst(src, m_IP);
      auto pktbuf       = pkt.Buffer();
      uint8_t queue_idx = pktbuf.sz / llarp::routing::ExitPadSize;
      auto& queue       = m_DownstreamQueues[queue_idx];
      if(queue.size() == 0)
      {
        queue.emplace_back();
        return queue.back().PutBuffer(buf, m_Counter++);
      }
      auto& msg = queue.back();
      if(msg.Size() + pktbuf.sz > llarp::routing::ExitPadSize)
      {
        queue.emplace_back();
        return queue.back().PutBuffer(pktbuf, m_Counter++);
      }
      else
        return msg.PutBuffer(pktbuf, m_Counter++);
    }

    bool
    Endpoint::Flush()
    {
      // flush upstream queue
      while(m_UpstreamQueue.size())
      {
        m_Parent->QueueOutboundTraffic(m_UpstreamQueue.top().pkt.ConstBuffer());
        m_UpstreamQueue.pop();
      }
      // flush downstream queue
      auto path = GetCurrentPath();
      bool sent = path != nullptr;
      if(path)
      {
        for(auto& item : m_DownstreamQueues)
        {
          auto& queue = item.second;
          while(queue.size())
          {
            auto& msg = queue.front();
            msg.S     = path->NextSeqNo();
            if(path->SendRoutingMessage(&msg, m_Parent->Router()))
            {
              m_RxRate += msg.Size();
              sent = true;
            }
            queue.pop_front();
          }
        }
      }
      for(auto& item : m_DownstreamQueues)
        item.second.clear();
      return sent;
    }

    llarp::path::IHopHandler*
    Endpoint::GetCurrentPath() const
    {
      auto router = m_Parent->Router();
      return router->paths.GetByUpstream(router->pubkey(), m_CurrentPath);
    }
  }  // namespace exit
}  // namespace llarp
