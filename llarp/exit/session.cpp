#include <exit/session.hpp>

#include <crypto/crypto.hpp>
#include <nodedb.hpp>
#include <path/path_context.hpp>
#include <path/path.hpp>
#include <router/abstractrouter.hpp>
#include <util/meta/memfn.hpp>
#include <utility>

namespace llarp
{
  namespace exit
  {
    BaseSession::BaseSession(
        const llarp::RouterID& routerId,
        std::function< bool(const llarp_buffer_t&) > writepkt,
        AbstractRouter* r, size_t numpaths, size_t hoplen, bool bundleRC)
        : llarp::path::Builder(r, numpaths, hoplen)
        , m_ExitRouter(routerId)
        , m_WritePacket(std::move(writepkt))
        , m_Counter(0)
        , m_LastUse(r->Now())
        , m_BundleRC(bundleRC)
    {
      CryptoManager::instance()->identity_keygen(m_ExitIdentity);
    }

    BaseSession::~BaseSession() = default;

    void
    BaseSession::HandlePathDied(path::Path_ptr p)
    {
      p->Rebuild();
    }

    util::StatusObject
    BaseSession::ExtractStatus() const
    {
      auto obj            = path::Builder::ExtractStatus();
      obj["lastExitUse"]  = to_json(m_LastUse);
      auto pub            = m_ExitIdentity.toPublic();
      obj["exitIdentity"] = pub.ToString();
      return obj;
    }

    bool
    BaseSession::LoadIdentityFromFile(const char* fname)
    {
      return m_ExitIdentity.LoadFromFile(fname);
    }

    bool
    BaseSession::ShouldBuildMore(llarp_time_t now) const
    {
      if(BuildCooldownHit(now))
        return false;
      const size_t expect = (1 + (numPaths / 2));
      // check 30 seconds into the future and see if we need more paths
      const llarp_time_t future = now + 30s + buildIntervalLimit;
      return NumPathsExistingAt(future) < expect;
    }

    void
    BaseSession::BlacklistSnode(const RouterID snode)
    {
      m_SnodeBlacklist.insert(std::move(snode));
    }

    bool
    BaseSession::SelectHop(llarp_nodedb* db, const std::set< RouterID >& prev,
                           RouterContact& cur, size_t hop,
                           llarp::path::PathRole roles)
    {
      std::set< RouterID > exclude = prev;
      for(const auto& snode : m_SnodeBlacklist)
      {
        if(snode != m_ExitRouter)
          exclude.insert(snode);
      }
      exclude.insert(m_ExitRouter);
      if(hop == numHops - 1)
      {
        if(db->Get(m_ExitRouter, cur))
          return true;
        m_router->LookupRouter(m_ExitRouter, nullptr);
        return false;
      }

      return path::Builder::SelectHop(db, exclude, cur, hop, roles);
    }

    bool
    BaseSession::CheckPathDead(path::Path_ptr, llarp_time_t dlt)
    {
      return dlt >= 10s;
    }

    void
    BaseSession::HandlePathBuilt(llarp::path::Path_ptr p)
    {
      path::Builder::HandlePathBuilt(p);
      p->SetDropHandler(util::memFn(&BaseSession::HandleTrafficDrop, this));
      p->SetDeadChecker(util::memFn(&BaseSession::CheckPathDead, this));
      p->SetExitTrafficHandler(util::memFn(&BaseSession::HandleTraffic, this));
      p->AddObtainExitHandler(util::memFn(&BaseSession::HandleGotExit, this));

      routing::ObtainExitMessage obtain;
      obtain.S = p->NextSeqNo();
      obtain.T = llarp::randint();
      PopulateRequest(obtain);
      if(!obtain.Sign(m_ExitIdentity))
      {
        llarp::LogError("Failed to sign exit request");
        return;
      }
      if(p->SendExitRequest(obtain, m_router))
        llarp::LogInfo("asking ", m_ExitRouter, " for exit");
      else
        llarp::LogError("failed to send exit request");
    }

    void
    BaseSession::AddReadyHook(SessionReadyFunc func)
    {
      m_PendingCallbacks.emplace_back(func);
    }

    bool
    BaseSession::HandleGotExit(llarp::path::Path_ptr p, llarp_time_t b)
    {
      if(b == 0s)
      {
        llarp::LogInfo("obtained an exit via ", p->Endpoint());
        CallPendingCallbacks(true);
      }
      return true;
    }

    void
    BaseSession::CallPendingCallbacks(bool success)
    {
      if(success)
      {
        auto self = shared_from_this();
        for(auto& f : m_PendingCallbacks)
          f(self);
      }
      else
      {
        for(auto& f : m_PendingCallbacks)
          f(nullptr);
      }
      m_PendingCallbacks.clear();
    }

    void
    BaseSession::ResetInternalState()
    {
      auto sendExitClose = [&](const llarp::path::Path_ptr p) {
        const static auto roles =
            llarp::path::ePathRoleExit | llarp::path::ePathRoleSVC;
        if(p->SupportsAnyRoles(roles))
        {
          llarp::LogInfo(p->Name(), " closing exit path");
          routing::CloseExitMessage msg;
          if(msg.Sign(m_ExitIdentity) && p->SendExitClose(msg, m_router))
          {
            p->ClearRoles(roles);
          }
          else
            llarp::LogWarn(p->Name(), " failed to send exit close message");
        }
      };
      ForEachPath(sendExitClose);
      path::Builder::ResetInternalState();
    }

    bool
    BaseSession::Stop()
    {
      CallPendingCallbacks(false);
      auto sendExitClose = [&](const path::Path_ptr p) {
        if(p->SupportsAnyRoles(path::ePathRoleExit))
        {
          LogInfo(p->Name(), " closing exit path");
          routing::CloseExitMessage msg;
          if(!(msg.Sign(m_ExitIdentity) && p->SendExitClose(msg, m_router)))
            LogWarn(p->Name(), " failed to send exit close message");
        }
      };
      ForEachPath(sendExitClose);
      m_router->pathContext().RemovePathSet(shared_from_this());
      return path::Builder::Stop();
    }

    bool
    BaseSession::HandleTraffic(llarp::path::Path_ptr, const llarp_buffer_t& buf,
                               uint64_t counter)
    {
      if(m_WritePacket)
      {
        llarp::net::IPPacket pkt;
        if(!pkt.Load(buf))
          return false;
        m_LastUse = m_router->Now();
        m_Downstream.emplace(counter, pkt);
        return true;
      }
      return false;
    }

    bool
    BaseSession::HandleTrafficDrop(llarp::path::Path_ptr p,
                                   const PathID_t& path, uint64_t s)
    {
      llarp::LogError("dropped traffic on exit ", m_ExitRouter, " S=", s,
                      " P=", path);
      p->EnterState(path::ePathIgnore, m_router->Now());
      return true;
    }

    bool
    BaseSession::QueueUpstreamTraffic(llarp::net::IPPacket pkt, const size_t N)
    {
      const auto pktbuf         = pkt.ConstBuffer();
      const llarp_buffer_t& buf = pktbuf;
      auto& queue               = m_Upstream[buf.sz / N];
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

      return back.PutBuffer(buf, m_Counter++);
    }

    bool
    BaseSession::IsReady() const
    {
      const size_t expect = (1 + (numPaths / 2));
      return AvailablePaths(llarp::path::ePathRoleExit) >= expect;
    }

    bool
    BaseSession::IsExpired(llarp_time_t now) const
    {
      return now > m_LastUse && now - m_LastUse > LifeSpan;
    }

    bool
    BaseSession::UrgentBuild(llarp_time_t now) const
    {
      if(BuildCooldownHit(now))
        return false;
      if(!IsReady())
        return NumInStatus(path::ePathBuilding) < numPaths;
      return path::Builder::UrgentBuild(now);
    }

    bool
    BaseSession::FlushUpstream()
    {
      auto now  = m_router->Now();
      auto path = PickRandomEstablishedPath(llarp::path::ePathRoleExit);
      if(path)
      {
        for(auto& item : m_Upstream)
        {
          auto& queue = item.second;  // XXX: uninitialised memory here!
          while(queue.size())
          {
            auto& msg = queue.front();
            if(path)
            {
              msg.S = path->NextSeqNo();
              path->SendRoutingMessage(msg, m_router);
            }
            queue.pop_front();

            // spread across all paths
            path = PickRandomEstablishedPath(llarp::path::ePathRoleExit);
          }
        }
      }
      else
      {
        if(m_Upstream.size())
          llarp::LogWarn("no path for exit session");
        // discard upstream
        for(auto& item : m_Upstream)
          item.second.clear();
        m_Upstream.clear();
        if(numHops == 1)
        {
          auto r = m_router;
          RouterContact rc;
          if(r->nodedb()->Get(m_ExitRouter, rc))
            r->TryConnectAsync(rc, 5);
          else
            r->LookupRouter(m_ExitRouter,
                            [r](const std::vector< RouterContact >& results) {
                              if(results.size())
                                r->TryConnectAsync(results[0], 5);
                            });
        }
        else if(UrgentBuild(now))
          BuildOneAlignedTo(m_ExitRouter);
      }
      return true;
    }

    void
    BaseSession::FlushDownstream()
    {
      while(m_Downstream.size())
      {
        if(m_WritePacket)
          m_WritePacket(m_Downstream.top().second.ConstBuffer());
        m_Downstream.pop();
      }
    }

    SNodeSession::SNodeSession(
        const llarp::RouterID& snodeRouter,
        std::function< bool(const llarp_buffer_t&) > writepkt,
        AbstractRouter* r, size_t numpaths, size_t hoplen,
        bool useRouterSNodeKey, bool bundleRC)
        : BaseSession(snodeRouter, writepkt, r, numpaths, hoplen, bundleRC)
    {
      if(useRouterSNodeKey)
      {
        m_ExitIdentity = r->identity();
      }
    }

    std::string
    SNodeSession::Name() const
    {
      return "SNode::" + m_ExitRouter.ToString();
    }

    std::string
    ExitSession::Name() const
    {
      return "Exit::" + m_ExitRouter.ToString();
    }
  }  // namespace exit
}  // namespace llarp
