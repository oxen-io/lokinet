#include <llarp/exit/session.hpp>
#include <llarp/path.hpp>
#include <router.hpp>

namespace llarp
{
  namespace exit
  {
    BaseSession::BaseSession(const llarp::RouterID& router,
                             std::function< bool(llarp_buffer_t) > writepkt,
                             llarp::Router* r, size_t numpaths, size_t hoplen)
        : llarp::path::Builder(r, r->dht, numpaths, hoplen)
        , m_ExitRouter(router)
        , m_WritePacket(writepkt)
        , m_Counter(0)
    {
      r->crypto.identity_keygen(m_ExitIdentity);
    }

    BaseSession::~BaseSession()
    {
    }

    bool
    BaseSession::ShouldBuildMore(llarp_time_t now) const
    {
      const size_t expect = (1 + (m_NumPaths / 2));
      if(NumPathsExistingAt(now + (10 * 1000)) < expect)
        return path::Builder::ShouldBuildMore(now);
      if(AvailablePaths(llarp::path::ePathRoleExit) < expect)
        return path::Builder::ShouldBuildMore(now);
      return false;
    }

    bool
    BaseSession::SelectHop(llarp_nodedb* db, const RouterContact& prev,
                           RouterContact& cur, size_t hop,
                           llarp::path::PathRole roles)
    {
      if(hop == numHops - 1)
      {
        return db->Get(m_ExitRouter, cur);
      }
      else
        return path::Builder::SelectHop(db, prev, cur, hop, roles);
    }

    void
    BaseSession::HandlePathBuilt(llarp::path::Path* p)
    {
      path::Builder::HandlePathBuilt(p);
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
      obtain.T = llarp::randint();
      PopulateRequest(obtain);
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
    BaseSession::QueueUpstreamTraffic(llarp::net::IPv4Packet pkt,
                                      const size_t N)
    {
      auto buf    = pkt.Buffer();
      auto& queue = m_Upstream[buf.sz / N];
      // queue overflow
      if(queue.size() >= MaxUpstreamQueueLength)
        return false;
      if(queue.size() == 0)
      {
        queue.emplace_back();
        return queue.back().PutBuffer(buf, m_Counter++);
      }
      auto& back = queue.back();
      // pack to nearest N
      if(back.Size() + buf.sz > N)
      {
        queue.emplace_back();
        return queue.back().PutBuffer(buf, m_Counter++);
      }
      else
        return back.PutBuffer(buf, m_Counter++);
    }

    bool
    BaseSession::IsReady() const
    {
      return AvailablePaths(llarp::path::ePathRoleExit) > 0;
    }

    bool
    BaseSession::FlushUpstreamTraffic()
    {
      auto path = PickRandomEstablishedPath(llarp::path::ePathRoleExit);
      if(!path)
      {
        // discard
        for(auto& item : m_Upstream)
          item.second.clear();
        return false;
      }
      for(auto& item : m_Upstream)
      {
        auto& queue = item.second;
        while(queue.size())
        {
          auto& msg = queue.front();
          msg.S     = path->NextSeqNo();
          path->SendRoutingMessage(&msg, router);
          queue.pop_front();
        }
      }
      return true;
    }

  }  // namespace exit
}  // namespace llarp
