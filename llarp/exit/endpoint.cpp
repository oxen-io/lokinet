#include <llarp/exit/endpoint.hpp>
#include "router.hpp"

namespace llarp
{
  namespace exit
  {
    Endpoint::Endpoint(const llarp::PubKey& remoteIdent,
                       const llarp::PathID_t& beginPath, bool rewriteIP,
                       llarp::handlers::ExitEndpoint* parent)
        : m_Parent(parent)
        , m_remoteSignKey(remoteIdent)
        , m_CurrentPath(beginPath)
        , m_IP(parent->ObtainIPForAddr(remoteIdent))
        , m_RewriteSource(rewriteIP)
    {
    }

    Endpoint::~Endpoint()
    {
      m_Parent->DelEndpointInfo(m_CurrentPath, m_IP, m_remoteSignKey);
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
      if(!m_Parent->QueueOutboundTraffic(std::move(pkt)))
      {
        llarp::LogError("failed to queue outbound traffic");
        return false;
      }
      m_TxRate += buf.sz;
      return true;
    }

    bool
    Endpoint::SendInboundTraffic(llarp_buffer_t buf)
    {
      auto path = GetCurrentPath();
      if(path)
      {
        llarp::routing::TransferTrafficMessage msg;
        if(!msg.PutBuffer(buf))
          return false;
        msg.S = path->NextSeqNo();
        if(!msg.Sign(m_Parent->Crypto(), m_Parent->Router()->identity))
          return false;
        if(!path->SendRoutingMessage(&msg, m_Parent->Router()))
          return false;
        m_RxRate += buf.sz;
        return true;
      }
      return false;
    }

    llarp::path::IHopHandler*
    Endpoint::GetCurrentPath() const
    {
      auto router = m_Parent->Router();
      return router->paths.GetByUpstream(router->pubkey(), m_CurrentPath);
    }
  }  // namespace exit
}  // namespace llarp