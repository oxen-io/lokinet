#include <llarp/exit/endpoint.hpp>
#include "router.hpp"

namespace llarp
{
  namespace exit
  {
    Endpoint::~Endpoint()
    {
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
        return path->SendRoutingMessage(&msg, m_Parent->Router());
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