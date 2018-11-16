#include <llarp/exit/session.hpp>
#include "router.hpp"

namespace llarp
{
  namespace exit
  {
    BaseSession::BaseSession(const llarp::RouterID& router,
                             std::function< bool(llarp_buffer_t) > writepkt,
                             llarp_router* r, size_t numpaths, size_t hoplen)
        : llarp::path::Builder(r, r->dht, numpaths, hoplen)
        , m_ExitRouter(router)
        , m_WritePacket(writepkt)
    {
      r->crypto.identity_keygen(m_ExitIdentity);
    }

    BaseSession::~BaseSession()
    {
    }

    bool
    BaseSession::ShouldBuildMore(llarp_time_t now) const
    {
      return AvailablePaths(llarp::path::ePathRoleExit) == 0
          || path::Builder::ShouldBuildMore(now);
    }

    bool
    BaseSession::SelectHop(llarp_nodedb* db, const RouterContact& prev,
                           RouterContact& cur, size_t hop,
                           llarp::path::PathRole roles)
    {
      if(hop == numHops - 1)
        return llarp_nodedb_get_rc(db, m_ExitRouter, cur);
      else
        return path::Builder::SelectHop(db, prev, cur, hop, roles);
    }

    void
    BaseSession::HandlePathBuilt(llarp::path::Path* p)
    {
      p->SetDropHandler(std::bind(&BaseSession::HandleTrafficDrop, this,
                                  std::placeholders::_1, std::placeholders::_2,
                                  std::placeholders::_3));
      p->SetExitTrafficHandler(std::bind(&BaseSession::HandleTraffic, this,
                                         std::placeholders::_1,
                                         std::placeholders::_2));
      p->AddObtainExitHandler(std::bind(&BaseSession::HandleGotExit, this,
                                        std::placeholders::_1,
                                        std::placeholders::_2));
      llarp::routing::ObtainExitMessage obtain;
      obtain.S = p->NextSeqNo();
      obtain.T = llarp_randint();
      obtain.X = 0;
      obtain.E = 1;
      if(!obtain.Sign(&router->crypto, m_ExitIdentity))
      {
        llarp::LogError("Failed to sign exit request");
        return;
      }
      if(p->SendExitRequest(&obtain, router))
        llarp::LogInfo("asking ", m_ExitRouter, " for exit");
      else
        llarp::LogError("faild to send exit request");
    }

    bool
    BaseSession::HandleGotExit(llarp::path::Path* p, llarp_time_t b)
    {
      if(b == 0)
      {
        llarp::LogInfo("obtained an exit via ", p->Endpoint());
      }
      return true;
    }

    bool
    BaseSession::HandleTraffic(llarp::path::Path* p, llarp_buffer_t pkt)
    {
      (void)p;
      if(m_WritePacket)
        return m_WritePacket(pkt);
      return false;
    }

    bool
    BaseSession::HandleTrafficDrop(llarp::path::Path* p, const PathID_t& path,
                                   uint64_t s)
    {
      (void)p;
      llarp::LogError("dropped traffic on exit ", m_ExitRouter, " S=", s,
                      " P=", path);
      return true;
    }

    bool
    BaseSession::SendUpstreamTraffic(llarp::net::IPv4Packet pkt)
    {
      auto path = PickRandomEstablishedPath(llarp::path::ePathRoleExit);
      if(!path)
        return false;
      llarp::routing::TransferTrafficMessage transfer;
      transfer.S = path->NextSeqNo();
      transfer.X.resize(pkt.sz);
      memcpy(transfer.X.data(), pkt.buf, pkt.sz);
      if(!transfer.Sign(&router->crypto, m_ExitIdentity))
        return false;
      return path->SendRoutingMessage(&transfer, router);
    }

  }  // namespace exit
}  // namespace llarp