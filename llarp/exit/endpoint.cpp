#include <llarp/exit/endpoint.hpp>
#include "router.hpp"

namespace llarp
{
  namespace exit
  {
    Endpoint::Endpoint(const llarp::PubKey& remoteIdent,
                       const llarp::PathID_t& beginPath, bool rewriteIP,
                       huint32_t ip, llarp::handlers::ExitEndpoint* parent)
        : m_Parent(parent)
        , m_remoteSignKey(remoteIdent)
        , m_CurrentPath(beginPath)
        , m_IP(ip)
        , m_RewriteSource(rewriteIP)
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

    bool Endpoint::LooksDead(llarp_time_t now, llarp_time_t timeout) const 
    {
      if(ExpiresSoon(now, timeout))
        return true;
      if (now > m_LastActive)
        return now - m_LastActive > timeout;
      return true;
    }

    bool
    Endpoint::SendOutboundTraffic(llarp_buffer_t buf)
    {
      llarp::net::IPv4Packet pkt;
      if(!pkt.Load(buf))
        return false;
      huint32_t dst;
      if(m_RewriteSource)
        dst = m_Parent->GetIfAddr();
      else
        dst = pkt.dst();
      pkt.UpdateIPv4PacketOnDst(m_IP, dst);
      if(!m_Parent->QueueOutboundTraffic(pkt.Buffer()))
      {
        llarp::LogError("failed to queue outbound traffic");
        return false;
      }
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

        if(m_DownstreamQueue.size() == 0)
          m_DownstreamQueue.emplace_back();
        auto pktbuf = pkt.Buffer();
        auto & msg = m_DownstreamQueue.back();
        if(msg.Size() + pktbuf.sz > llarp::routing::ExitPadSize)
        {
          m_DownstreamQueue.emplace_back();
          return m_DownstreamQueue.back().PutBuffer(pktbuf);
        }
        else
          return msg.PutBuffer(pktbuf);
    }

    bool 
    Endpoint::FlushInboundTraffic()
    {
      auto path = GetCurrentPath();
      bool sent = m_DownstreamQueue.size() == 0;
      if(path)
      {
        for(auto & msg : m_DownstreamQueue)
        {
          msg.S = path->NextSeqNo();
          if(path->SendRoutingMessage(&msg, m_Parent->Router()))
          {
            m_RxRate += msg.Size();
            sent = true;
          }
        }
      }
      m_DownstreamQueue.clear();
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