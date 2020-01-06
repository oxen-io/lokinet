#include <router/outbound_session_maker.hpp>

#include <link/server.hpp>
#include <router_contact.hpp>
#include <nodedb.hpp>
#include <router/i_rc_lookup_handler.hpp>
#include <link/i_link_manager.hpp>
#include <util/meta/memfn.hpp>
#include <util/thread/logic.hpp>
#include <util/thread/threading.hpp>
#include <util/status.hpp>
#include <crypto/crypto.hpp>
#include <utility>

namespace llarp
{
  struct PendingSession
  {
    // TODO: add session establish status metadata, e.g. num retries

    const RouterContact rc;
    LinkLayer_ptr link;

    size_t attemptCount = 0;

    PendingSession(RouterContact _rc, LinkLayer_ptr _link)
        : rc(std::move(_rc)), link(std::move(_link))
    {
    }
  };

  bool
  OutboundSessionMaker::OnSessionEstablished(ILinkSession *session)
  {
    // TODO: do we want to keep it

    const auto router = RouterID(session->GetPubKey());

    const std::string remoteType =
        session->GetRemoteRC().IsPublicRouter() ? "router" : "client";
    LogInfo("session with ", remoteType, " [", router, "] established");

    if(not _rcLookup->RemoteIsAllowed(router))
    {
      FinalizeRequest(router, SessionResult::InvalidRouter);
      return false;
    }

    auto func = std::bind(&OutboundSessionMaker::VerifyRC, this,
                          session->GetRemoteRC());
    _threadpool->addJob(func);

    return true;
  }

  void
  OutboundSessionMaker::OnConnectTimeout(ILinkSession *session)
  {
    // TODO: retry/num attempts

    LogWarn("Session establish attempt to ", RouterID(session->GetPubKey()),
            " timed out.");
    FinalizeRequest(session->GetPubKey(), SessionResult::Timeout);
  }

  void
  OutboundSessionMaker::CreateSessionTo(const RouterID &router,
                                        RouterCallback on_result)
  {
    if(on_result)
    {
      util::Lock l(&_mutex);

      auto itr_pair = pendingCallbacks.emplace(router, CallbacksQueue{});
      itr_pair.first->second.push_back(on_result);
    }

    if(HavePendingSessionTo(router))
    {
      return;
    }

    CreatePendingSession(router);

    LogDebug("Creating session establish attempt to ", router, " .");

    auto fn = util::memFn(&OutboundSessionMaker::OnRouterContactResult, this);

    _rcLookup->GetRC(router, fn);
  }

  void
  OutboundSessionMaker::CreateSessionTo(const RouterContact &rc,
                                        RouterCallback on_result)
  {
    if(on_result)
    {
      util::Lock l(&_mutex);

      auto itr_pair = pendingCallbacks.emplace(rc.pubkey, CallbacksQueue{});
      itr_pair.first->second.push_back(on_result);
    }

    if(not HavePendingSessionTo(rc.pubkey))
    {
      LogDebug("Creating session establish attempt to ", rc.pubkey, " .");
      CreatePendingSession(rc.pubkey);
    }

    GotRouterContact(rc.pubkey, rc);
  }

  bool
  OutboundSessionMaker::HavePendingSessionTo(const RouterID &router) const
  {
    util::Lock l(&_mutex);
    return pendingSessions.find(router) != pendingSessions.end();
  }

  void
  OutboundSessionMaker::ConnectToRandomRouters(int numDesired)
  {
    int remainingDesired = numDesired;

    _nodedb->visit([&](const RouterContact &other) -> bool {
      if(!_rcLookup->RemoteIsAllowed(other.pubkey))
      {
        return remainingDesired > 0;
      }
      if(randint() % 2 == 0
         && !(_linkManager->HasSessionTo(other.pubkey)
              || HavePendingSessionTo(other.pubkey)))
      {
        CreateSessionTo(other, nullptr);
        --remainingDesired;
      }
      return remainingDesired > 0;
    });
    LogDebug("connecting to ", numDesired - remainingDesired, " out of ",
             numDesired, " random routers");
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
      ILinkManager *linkManager, I_RCLookupHandler *rcLookup,
      Profiling *profiler, std::shared_ptr< Logic > logic, llarp_nodedb *nodedb,
      std::shared_ptr< llarp::thread::ThreadPool > threadpool)
  {
    _linkManager = linkManager;
    _rcLookup    = rcLookup;
    _logic       = logic;
    _nodedb      = nodedb;
    _threadpool  = threadpool;
    _profiler    = profiler;
  }

  void
  OutboundSessionMaker::DoEstablish(const RouterID &router)
  {
    util::ReleasableLock l(&_mutex);

    auto itr = pendingSessions.find(router);

    if(itr == pendingSessions.end())
    {
      return;
    }

    const auto &job = itr->second;
    if(!job->link->TryEstablishTo(job->rc))
    {
      // TODO: maybe different failure type?

      l.Release();
      FinalizeRequest(router, SessionResult::NoLink);
    }
  }

  void
  OutboundSessionMaker::GotRouterContact(const RouterID &router,
                                         const RouterContact &rc)
  {
    {
      util::ReleasableLock l(&_mutex);

      // in case other request found RC for this router after this request was
      // made
      auto itr = pendingSessions.find(router);
      if(itr == pendingSessions.end())
      {
        return;
      }

      LinkLayer_ptr link = _linkManager->GetCompatibleLink(rc);

      if(!link)
      {
        l.Release();
        FinalizeRequest(router, SessionResult::NoLink);
        return;
      }

      auto session = std::make_shared< PendingSession >(rc, link);

      itr->second = session;
    }
    if(ShouldConnectTo(router))
    {
      auto fn = std::bind(&OutboundSessionMaker::DoEstablish, this, router);
      LogicCall(_logic, fn);
    }
  }

  bool
  OutboundSessionMaker::ShouldConnectTo(const RouterID &router) const
  {
    if(router == us)
      return false;
    size_t numPending = 0;
    {
      util::Lock lock(&_mutex);
      if(pendingSessions.find(router) == pendingSessions.end())
        numPending += pendingSessions.size();
    }
    if(_linkManager->HasSessionTo(router))
      return false;
    return _linkManager->NumberOfConnectedRouters() + numPending
        < maxConnectedRouters;
  }

  void
  OutboundSessionMaker::InvalidRouter(const RouterID &router)
  {
    FinalizeRequest(router, SessionResult::InvalidRouter);
  }

  void
  OutboundSessionMaker::RouterNotFound(const RouterID &router)
  {
    FinalizeRequest(router, SessionResult::RouterNotFound);
  }

  void
  OutboundSessionMaker::OnRouterContactResult(const RouterID &router,
                                              const RouterContact *const rc,
                                              const RCRequestResult result)
  {
    if(not HavePendingSessionTo(router))
    {
      return;
    }

    switch(result)
    {
      case RCRequestResult::Success:
        if(rc)
        {
          GotRouterContact(router, *rc);
        }
        else
        {
          LogError("RCRequestResult::Success but null rc pointer given");
        }
        break;
      case RCRequestResult::InvalidRouter:
        InvalidRouter(router);
        break;
      case RCRequestResult::RouterNotFound:
        RouterNotFound(router);
        break;
      default:
        break;
    }
  }

  void
  OutboundSessionMaker::VerifyRC(const RouterContact rc)
  {
    if(not _rcLookup->CheckRC(rc))
    {
      FinalizeRequest(rc.pubkey, SessionResult::InvalidRouter);
      return;
    }

    FinalizeRequest(rc.pubkey, SessionResult::Establish);
  }

  void
  OutboundSessionMaker::CreatePendingSession(const RouterID &router)
  {
    util::Lock l(&_mutex);
    pendingSessions.emplace(router, nullptr);
  }

  void
  OutboundSessionMaker::FinalizeRequest(const RouterID &router,
                                        const SessionResult type)
  {
    CallbacksQueue movedCallbacks;
    {
      util::Lock l(&_mutex);

      if(type == SessionResult::Establish)
      {
        _profiler->MarkConnectSuccess(router);
      }
      else
      {
        // TODO: add non timeout related fail case
        _profiler->MarkConnectTimeout(router);
      }

      auto itr = pendingCallbacks.find(router);

      if(itr != pendingCallbacks.end())
      {
        movedCallbacks.splice(movedCallbacks.begin(), itr->second);
        pendingCallbacks.erase(itr);
      }
    }

    for(const auto &callback : movedCallbacks)
    {
      auto func = std::bind(callback, router, type);
      LogicCall(_logic, func);
    }

    {
      util::Lock l(&_mutex);
      pendingSessions.erase(router);
    }
  }

}  // namespace llarp
