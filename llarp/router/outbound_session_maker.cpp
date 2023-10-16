#include "outbound_session_maker.hpp"

#include "router.hpp"
#include <llarp/tooling/peer_stats_event.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/nodedb.hpp>
#include "rc_lookup_handler.hpp"
#include <llarp/link/link_manager.hpp>
#include <llarp/util/meta/memfn.hpp>
#include <llarp/util/thread/threading.hpp>
#include <llarp/util/status.hpp>
#include <llarp/crypto/crypto.hpp>
#include <utility>

#include <llarp/rpc/lokid_rpc_client.hpp>

namespace llarp
{

  bool
  OutboundSessionMaker::OnSessionEstablished(AbstractLinkSession* session)
  {
    // TODO: do we want to keep it
    const RouterContact rc = session->GetRemoteRC();
    const auto router = RouterID(session->GetPubKey());
    const bool isOutbound = not session->IsInbound();
    const std::string remoteType = rc.IsPublicRouter() ? "router" : "client";
    LogInfo(
        "session with ", remoteType, " [", router, "] ", isOutbound ? "established" : "received");

    if (not _rcLookup->SessionIsAllowed(router))
    {
      FinalizeRequest(router, SessionResult::InvalidRouter);
      return false;
    }

    if (isOutbound)
    {
      work([this, rc] { VerifyRC(rc); });
      return true;
    }
    return _rcLookup->CheckRC(rc);
  }

  void
  OutboundSessionMaker::OnConnectTimeout(AbstractLinkSession* session)
  {
    const auto router = RouterID(session->GetPubKey());
    LogWarn("Session establish attempt to ", router, " timed out.", session->GetRemoteEndpoint());
    FinalizeRequest(router, SessionResult::Timeout);
  }

  void
  OutboundSessionMaker::CreateSessionTo(const RouterID& router, RouterCallback on_result)
  {
    {
      util::Lock l(_mutex);

      auto itr_pair = pendingCallbacks.emplace(router, CallbacksQueue{});
      if (on_result)
        itr_pair.first->second.push_back(on_result);
    }

    if (HavePendingSessionTo(router))
    {
      LogDebug("has pending session to", router);
      return;
    }

    CreatePendingSession(router);

    // short-circuit to success callback if we already have an outbound session
    // to the remote
    if (_linkManager->HasConnection(router))
    {
      FinalizeRequest(router, SessionResult::Establish);
      return;
    }

    LogDebug("Creating session establish attempt to ", router, " .");

    auto fn = util::memFn(&OutboundSessionMaker::OnRouterContactResult, this);

    _rcLookup->GetRC(router, fn);
  }

  void
  OutboundSessionMaker::CreateSessionTo(const RouterContact& rc, RouterCallback on_result)
  {
    const RouterID router{rc.pubkey};

    {
      util::Lock l(_mutex);

      auto itr_pair = pendingCallbacks.emplace(router, CallbacksQueue{});
      if (on_result)
        itr_pair.first->second.push_back(on_result);
    }

    if (not HavePendingSessionTo(router))
    {
      LogDebug("Creating session establish attempt to ", router);
      CreatePendingSession(router);
    }

    GotRouterContact(router, rc);
  }

  bool
  OutboundSessionMaker::HavePendingSessionTo(const RouterID& router) const
  {
    util::Lock l(_mutex);
    return pendingCallbacks.find(router) != pendingCallbacks.end();
  }

  void
  OutboundSessionMaker::ConnectToRandomRouters(int numDesired)
  {
    int remainingDesired = numDesired;
    std::set<RouterID> exclude;
    do
    {
      auto filter = [exclude](const auto& rc) -> bool { return exclude.count(rc.pubkey) == 0; };

      RouterContact other;
      if (const auto maybe = _nodedb->GetRandom(filter))
      {
        other = *maybe;
      }
      else
        break;

      exclude.insert(other.pubkey);
      if (not _rcLookup->SessionIsAllowed(other.pubkey))
      {
        continue;
      }
      if (not(_linkManager->HasSessionTo(other.pubkey) || HavePendingSessionTo(other.pubkey)))
      {
        CreateSessionTo(other, nullptr);
        --remainingDesired;
      }

    } while (remainingDesired > 0);
    LogDebug(
        "connecting to ", numDesired - remainingDesired, " out of ", numDesired, " random routers");
  }

  // TODO: this
  util::StatusObject
  OutboundSessionMaker::ExtractStatus() const
  {
    util::StatusObject status{};
    return status;
  }

  void
  OutboundSessionMaker::Init(
      Router* router,
      LinkManager* linkManager,
      RCLookupHandler* rcLookup,
      Profiling* profiler,
      EventLoop_ptr loop,
      WorkerFunc_t dowork)
  {
    _router = router;
    _linkManager = linkManager;
    _rcLookup = rcLookup;
    _loop = std::move(loop);
    _nodedb = router->nodedb();
    _profiler = profiler;
    work = std::move(dowork);
  }

  void
  OutboundSessionMaker::GotRouterContact(const RouterID& router, const RouterContact& rc)
  {
    if (not _rcLookup->CheckRC(rc))
    {
      FinalizeRequest(rc.pubkey, SessionResult::InvalidRouter);
      return;
    }

    if (not ShouldConnectTo(router))
    {
      FinalizeRequest(router, SessionResult::NoLink);
      return;
    }

    auto result = _linkManager->Connect(rc);
    if (result)
      FinalizeRequest(router, SessionResult::Establish);
    else
      FinalizeRequest(router, SessionResult::EstablishFail);
  }

  bool
  OutboundSessionMaker::ShouldConnectTo(const RouterID& router) const
  {
    if (router == us or not _rcLookup->SessionIsAllowed(router))
      return false;
    if (_router->IsServiceNode())
      return true;

    size_t numPending = 0;
    {
      util::Lock lock(_mutex);
      if (pendingCallbacks.find(router) == pendingCallbacks.end())
        numPending += pendingCallbacks.size();
    }

    return _linkManager->get_num_connected() + numPending < maxConnectedRouters;
  }

  void
  OutboundSessionMaker::InvalidRouter(const RouterID& router)
  {
    FinalizeRequest(router, SessionResult::InvalidRouter);
  }

  void
  OutboundSessionMaker::RouterNotFound(const RouterID& router)
  {
    FinalizeRequest(router, SessionResult::RouterNotFound);
  }

  void
  OutboundSessionMaker::OnRouterContactResult(
      const RouterID& router, const RouterContact* const rc, const RCRequestResult result)
  {
    if (not HavePendingSessionTo(router))
    {
      LogError("no pending session to ", router);
      return;
    }

    switch (result)
    {
      case RCRequestResult::Success:
        if (rc)
        {
          GotRouterContact(router, *rc);
        }
        else
        {
          LogError("RCRequestResult::Success but null rc pointer given");
          InvalidRouter(router);
        }
        break;
      case RCRequestResult::InvalidRouter:
        InvalidRouter(router);
        break;
      case RCRequestResult::RouterNotFound:
        RouterNotFound(router);
        break;
      default:
        RouterNotFound(router);
        break;
    }
  }

  void
  OutboundSessionMaker::VerifyRC(const RouterContact rc)
  {
    if (not _rcLookup->CheckRC(rc))
    {
      FinalizeRequest(rc.pubkey, SessionResult::InvalidRouter);
      return;
    }

    FinalizeRequest(rc.pubkey, SessionResult::Establish);
  }

  // TODO: rename this, if we even want to keep it
  void
  OutboundSessionMaker::CreatePendingSession(const RouterID& router)
  {
    auto peerDb = _router->peerDb();
    if (peerDb)
    {
      peerDb->modifyPeerStats(router, [](PeerStats& stats) { stats.numConnectionAttempts++; });
    }

    _router->NotifyRouterEvent<tooling::ConnectionAttemptEvent>(_router->pubkey(), router);
  }

  void
  OutboundSessionMaker::FinalizeRequest(const RouterID& router, const SessionResult type)
  {
    CallbacksQueue movedCallbacks;
    {
      util::Lock l(_mutex);

      if (type == SessionResult::Establish)
      {
        _profiler->MarkConnectSuccess(router);
      }
      else
      {
        // TODO: add non timeout related fail case
        _profiler->MarkConnectTimeout(router);
      }

      auto itr = pendingCallbacks.find(router);

      if (itr != pendingCallbacks.end())
      {
        movedCallbacks.splice(movedCallbacks.begin(), itr->second);
        pendingCallbacks.erase(itr);
      }
    }

    for (const auto& callback : movedCallbacks)
    {
      _loop->call([callback, router, type] { return callback(router, type); });
    }
  }

}  // namespace llarp
