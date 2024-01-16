#include "session.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/path/path.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/router/router.hpp>

#include <utility>

namespace llarp::exit
{
  BaseSession::BaseSession(
      const llarp::RouterID& routerId,
      std::function<bool(const llarp_buffer_t&)> writepkt,
      Router* r,
      size_t numpaths,
      size_t hoplen,
      EndpointBase* parent)
      : llarp::path::Builder{r, numpaths, hoplen}
      , exit_router{routerId}
      , packet_write_func{std::move(writepkt)}
      , _last_use{r->now()}
      , _parent{parent}
  {
    (void)_parent;
    crypto::identity_keygen(exit_key);
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
    auto obj = path::Builder::ExtractStatus();
    obj["lastExitUse"] = to_json(_last_use);
    auto pub = exit_key.toPublic();
    obj["exitIdentity"] = pub.ToString();
    obj["endpoint"] = exit_router.ToString();
    return obj;
  }

  bool
  BaseSession::LoadIdentityFromFile(const char* fname)
  {
    return exit_key.LoadFromFile(fname);
  }

  bool
  BaseSession::ShouldBuildMore(llarp_time_t now) const
  {
    if (BuildCooldownHit(now))
      return false;
    const size_t expect = (1 + (numDesiredPaths / 2));
    // check 30 seconds into the future and see if we need more paths
    const llarp_time_t future = now + 30s + buildIntervalLimit;
    return NumPathsExistingAt(future) < expect;
  }

  void
  BaseSession::BlacklistSNode(const RouterID snode)
  {
    snode_blacklist.insert(std::move(snode));
  }

  std::optional<std::vector<RemoteRC>>
  BaseSession::GetHopsForBuild()
  {
    if (numHops == 1)
    {
      if (auto maybe = router->node_db()->get_rc(exit_router))
        return std::vector<RemoteRC>{*maybe};
      return std::nullopt;
    }

    return GetHopsAlignedToForBuild(exit_router);
  }

  bool
  BaseSession::CheckPathDead(path::Path_ptr, llarp_time_t dlt)
  {
    return dlt >= path::ALIVE_TIMEOUT;
  }

  void
  BaseSession::HandlePathBuilt(llarp::path::Path_ptr p)
  {
    path::Builder::HandlePathBuilt(p);
    // p->SetDropHandler(util::memFn(&BaseSession::HandleTrafficDrop, this));
    // p->SetDeadChecker(util::memFn(&BaseSession::CheckPathDead, this));
    // p->SetExitTrafficHandler(util::memFn(&BaseSession::HandleTraffic, this));
    // p->AddObtainExitHandler(util::memFn(&BaseSession::HandleGotExit, this));

    // TODO: add callback here
    if (p->obtain_exit(
            exit_key, std::is_same_v<decltype(p), ExitSession> ? 1 : 0, p->TXID().bt_encode()))
      log::info(link_cat, "Asking {} for exit", exit_router);
    else
      log::warning(link_cat, "Failed to send exit request");
  }

  void
  BaseSession::AddReadyHook(SessionReadyFunc func)
  {
    m_PendingCallbacks.emplace_back(func);
  }

  bool
  BaseSession::HandleGotExit(llarp::path::Path_ptr p, llarp_time_t b)
  {
    if (b == 0s)
    {
      llarp::LogInfo("obtained an exit via ", p->Endpoint());
      m_CurrentPath = p->RXID();
      CallPendingCallbacks(true);
    }
    return true;
  }

  void
  BaseSession::CallPendingCallbacks(bool success)
  {
    if (m_PendingCallbacks.empty())
      return;

    if (success)
    {
      auto self = shared_from_this();
      for (auto& f : m_PendingCallbacks)
        f(self);
    }
    else
    {
      for (auto& f : m_PendingCallbacks)
        f(nullptr);
    }
    m_PendingCallbacks.clear();
  }

  void
  BaseSession::ResetInternalState()
  {
    auto sendExitClose = [&](const llarp::path::Path_ptr p) {
      const static auto roles = llarp::path::ePathRoleExit | llarp::path::ePathRoleSVC;
      if (p->SupportsAnyRoles(roles))
      {
        log::info(link_cat, "{} closing exit path", p->name());
        if (p->close_exit(exit_key, p->TXID().bt_encode()))
          p->ClearRoles(roles);
        else
          llarp::LogWarn(p->name(), " failed to send exit close message");
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
      if (p->SupportsAnyRoles(path::ePathRoleExit))
      {
        LogInfo(p->name(), " closing exit path");
        // TODO: add callback here

        if (p->close_exit(exit_key, p->TXID().bt_encode()))
          log::info(link_cat, "");
        else
          log::warning(link_cat, "{} failed to send exit close message", p->name());
      }
    };
    ForEachPath(sendExitClose);
    router->path_context().RemovePathSet(shared_from_this());
    return path::Builder::Stop();
  }

  bool
  BaseSession::HandleTraffic(
      llarp::path::Path_ptr, const llarp_buffer_t&, uint64_t, service::ProtocolType)
  {
    // const service::ConvoTag tag{path->RXID().as_array()};

    // if (t == service::ProtocolType::QUIC)
    // {
    //   auto quic = m_Parent->GetQUICTunnel();
    //   if (not quic)
    //     return false;
    //   quic->receive_packet(tag, buf);
    //   return true;
    // }

    // if (packet_write_func)
    // {
    //   llarp::net::IPPacket pkt{buf.view_all()};
    //   if (pkt.empty())
    //     return false;
    //   _last_use = router->now();
    //   m_Downstream.emplace(counter, pkt);
    //   return true;
    // }
    return false;
  }

  bool
  BaseSession::HandleTrafficDrop(llarp::path::Path_ptr p, const PathID_t& path, uint64_t s)
  {
    llarp::LogError("dropped traffic on exit ", exit_router, " S=", s, " P=", path);
    p->EnterState(path::IGNORE, router->now());
    return true;
  }

  bool
  BaseSession::IsReady() const
  {
    if (m_CurrentPath.IsZero())
      return false;
    const size_t expect = (1 + (numDesiredPaths / 2));
    return AvailablePaths(llarp::path::ePathRoleExit) >= expect;
  }

  bool
  BaseSession::IsExpired(llarp_time_t now) const
  {
    return now > _last_use && now - _last_use > LifeSpan;
  }

  bool
  BaseSession::UrgentBuild(llarp_time_t now) const
  {
    if (BuildCooldownHit(now))
      return false;
    if (IsReady() and NumInStatus(path::BUILDING) < numDesiredPaths)
      return path::Builder::UrgentBuild(now);
    return false;
  }

  bool
  BaseSession::FlushUpstream()
  {
    // auto now = router->now();
    // auto path = PickEstablishedPath(llarp::path::ePathRoleExit);
    // if (path)
    // {
    //   for (auto& [i, queue] : m_Upstream)
    //   {
    //     while (queue.size())
    //     {
    //       auto& msg = queue.front();
    //       msg.sequence_number = path->NextSeqNo();
    //       path->SendRoutingMessage(msg, router);
    //       queue.pop_front();
    //     }
    //   }
    // }
    // else
    // {
    //   if (m_Upstream.size())
    //     llarp::LogWarn("no path for exit session");
    //   // discard upstream
    //   for (auto& [i, queue] : m_Upstream)
    //     queue.clear();
    //   m_Upstream.clear();

    //   if (numHops == 1)
    //   {
    //     if (const auto maybe = router->node_db()->get_rc(exit_router); maybe.has_value())
    //       router->connect_to(*maybe);
    //   }
    //   else if (UrgentBuild(now))
    //     BuildOneAlignedTo(exit_router);
    // }
    return true;
  }

  void
  BaseSession::FlushDownstream()
  {
    // while (m_Downstream.size())
    // {
    //   if (packet_write_func)
    //     packet_write_func(const_cast<net::IPPacket&>(m_Downstream.top().second).steal());
    //   m_Downstream.pop();
    // }
  }

  SNodeSession::SNodeSession(
      const llarp::RouterID& snodeRouter,
      std::function<bool(const llarp_buffer_t&)> writepkt,
      Router* r,
      size_t numpaths,
      size_t hoplen,
      bool useRouterSNodeKey,
      EndpointBase* parent)
      : BaseSession{snodeRouter, writepkt, r, numpaths, hoplen, parent}
  {
    if (useRouterSNodeKey)
    {
      exit_key = r->identity();
    }
  }

  std::string
  SNodeSession::Name() const
  {
    return "SNode::" + exit_router.ToString();
  }

  std::string
  ExitSession::Name() const
  {
    return "Exit::" + exit_router.ToString();
  }

  void
  ExitSession::send_packet_to_remote(std::string buf)
  {
    if (buf.empty())
      return;

    if (auto path = PickEstablishedPath(llarp::path::ePathRoleExit))
    {}
    else
    {}
  }

  void
  SNodeSession::send_packet_to_remote(std::string buf)
  {
    if (buf.empty())
      return;

    if (auto path = PickEstablishedPath(llarp::path::ePathRoleExit))
    {
      //
    }
    else
    {}
  }
}  // namespace llarp::exit
