#include "session.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/path/path.hpp>
#include <llarp/quic/tunnel.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/meta/memfn.hpp>
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
      , m_Counter{0}
      , m_LastUse{r->now()}
      , m_BundleRC{false}
      , m_Parent{parent}
  {
    CryptoManager::instance()->identity_keygen(exit_key);
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
    obj["lastExitUse"] = to_json(m_LastUse);
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

  std::optional<std::vector<RouterContact>>
  BaseSession::GetHopsForBuild()
  {
    if (numHops == 1)
    {
      if (auto maybe = router->node_db()->get_rc(exit_router))
        return std::vector<RouterContact>{*maybe};
      return std::nullopt;
    }
    else
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
        routing::CloseExitMessage msg;
        if (!(msg.Sign(exit_key) && p->SendExitClose(msg, router)))
          LogWarn(p->name(), " failed to send exit close message");
      }
    };
    ForEachPath(sendExitClose);
    router->path_context().RemovePathSet(shared_from_this());
    return path::Builder::Stop();
  }

  bool
  BaseSession::HandleTraffic(
      llarp::path::Path_ptr path,
      const llarp_buffer_t& buf,
      uint64_t counter,
      service::ProtocolType t)
  {
    const service::ConvoTag tag{path->RXID().as_array()};

    if (t == service::ProtocolType::QUIC)
    {
      auto quic = m_Parent->GetQUICTunnel();
      if (not quic)
        return false;
      quic->receive_packet(tag, buf);
      return true;
    }

    if (packet_write_func)
    {
      llarp::net::IPPacket pkt{buf.view_all()};
      if (pkt.empty())
        return false;
      m_LastUse = router->now();
      m_Downstream.emplace(counter, pkt);
      return true;
    }
    return false;
  }

  bool
  BaseSession::HandleTrafficDrop(llarp::path::Path_ptr p, const PathID_t& path, uint64_t s)
  {
    llarp::LogError("dropped traffic on exit ", exit_router, " S=", s, " P=", path);
    p->EnterState(path::ePathIgnore, router->now());
    return true;
  }

  bool
  BaseSession::QueueUpstreamTraffic(
      llarp::net::IPPacket pkt, const size_t N, service::ProtocolType t)
  {
    auto& queue = m_Upstream[pkt.size() / N];
    // queue overflow
    if (queue.size() >= MaxUpstreamQueueLength)
      return false;
    if (queue.size() == 0)
    {
      queue.emplace_back();
      queue.back().protocol = t;
      return queue.back().PutBuffer(llarp_buffer_t{pkt}, m_Counter++);
    }
    auto& back = queue.back();
    // pack to nearest N
    if (back.Size() + pkt.size() > N)
    {
      queue.emplace_back();
      queue.back().protocol = t;
      return queue.back().PutBuffer(llarp_buffer_t{pkt}, m_Counter++);
    }
    back.protocol = t;
    return back.PutBuffer(llarp_buffer_t{pkt}, m_Counter++);
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
    return now > m_LastUse && now - m_LastUse > LifeSpan;
  }

  bool
  BaseSession::UrgentBuild(llarp_time_t now) const
  {
    if (BuildCooldownHit(now))
      return false;
    if (IsReady() and NumInStatus(path::ePathBuilding) < numDesiredPaths)
      return path::Builder::UrgentBuild(now);
    return false;
  }

  bool
  BaseSession::FlushUpstream()
  {
    auto now = router->now();
    auto path = PickEstablishedPath(llarp::path::ePathRoleExit);
    if (path)
    {
      for (auto& [i, queue] : m_Upstream)
      {
        while (queue.size())
        {
          auto& msg = queue.front();
          msg.sequence_number = path->NextSeqNo();
          path->SendRoutingMessage(msg, router);
          queue.pop_front();
        }
      }
    }
    else
    {
      if (m_Upstream.size())
        llarp::LogWarn("no path for exit session");
      // discard upstream
      for (auto& [i, queue] : m_Upstream)
        queue.clear();
      m_Upstream.clear();
      if (numHops == 1)
      {
        auto r = router;
        if (const auto maybe = r->node_db()->get_rc(exit_router); maybe.has_value())
          r->connect_to(*maybe);
        else
          r->lookup_router(exit_router, [r](oxen::quic::message m) mutable {
            if (m)
            {
              std::string payload;

              try
              {
                oxenc::bt_dict_consumer btdc{m.body()};
                payload = btdc.require<std::string>("RC");
              }
              catch (...)
              {
                log::warning(link_cat, "Failed to parse Find Router response!");
                throw;
              }

              RouterContact result{std::move(payload)};
              r->node_db()->put_rc_if_newer(result);
              r->connect_to(result);
            }
            else
            {
              r->link_manager().handle_find_router_error(std::move(m));
            }
          });
      }
      else if (UrgentBuild(now))
        BuildOneAlignedTo(exit_router);
    }
    return true;
  }

  void
  BaseSession::FlushDownstream()
  {
    while (m_Downstream.size())
    {
      if (packet_write_func)
        packet_write_func(const_cast<net::IPPacket&>(m_Downstream.top().second).steal());
      m_Downstream.pop();
    }
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
  SNodeSession::SendPacketToRemote(const llarp_buffer_t& buf, service::ProtocolType t)
  {
    net::IPPacket pkt{buf.view_all()};
    if (pkt.empty())
      return;
    pkt.ZeroAddresses();
    QueueUpstreamTraffic(std::move(pkt), llarp::routing::EXIT_PAD_SIZE, t);
  }

  void
  ExitSession::SendPacketToRemote(const llarp_buffer_t& buf, service::ProtocolType t)
  {
    net::IPPacket pkt{buf.view_all()};
    if (pkt.empty())
      return;

    pkt.ZeroSourceAddress();
    QueueUpstreamTraffic(std::move(pkt), llarp::routing::EXIT_PAD_SIZE, t);
  }
}  // namespace llarp::exit
