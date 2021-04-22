#pragma once

#include "i_outbound_session_maker.hpp"

#include "i_rc_lookup_handler.hpp"
#include <llarp/util/thread/threading.hpp>

#include <llarp/profiling.hpp>

#include <unordered_map>
#include <list>
#include <memory>

namespace llarp
{
  struct PendingSession;

  struct ILinkManager;
  struct I_RCLookupHandler;

  struct OutboundSessionMaker final : public IOutboundSessionMaker
  {
    using Work_t = std::function<void(void)>;
    using WorkerFunc_t = std::function<void(Work_t)>;

    using CallbacksQueue = std::list<RouterCallback>;

   public:
    ~OutboundSessionMaker() override = default;

    bool
    OnSessionEstablished(ILinkSession* session) override;

    void
    OnConnectTimeout(ILinkSession* session) override;

    void
    CreateSessionTo(const RouterID& router, RouterCallback on_result) override EXCLUDES(_mutex);

    void
    CreateSessionTo(const RouterContact& rc, RouterCallback on_result) override EXCLUDES(_mutex);

    bool
    HavePendingSessionTo(const RouterID& router) const override EXCLUDES(_mutex);

    void
    ConnectToRandomRouters(int numDesired) override;

    util::StatusObject
    ExtractStatus() const override;

    bool
    ShouldConnectTo(const RouterID& router) const override EXCLUDES(_mutex);

    void
    Init(
        AbstractRouter* router,
        ILinkManager* linkManager,
        I_RCLookupHandler* rcLookup,
        Profiling* profiler,
        EventLoop_ptr loop,
        WorkerFunc_t work);

    void
    SetOurRouter(RouterID r)
    {
      us = std::move(r);
    }

    /// always maintain this many connections to other routers
    size_t minConnectedRouters = 4;
    /// hard upperbound limit on the number of router to router connections
    size_t maxConnectedRouters = 6;

   private:
    void
    DoEstablish(const RouterID& router) EXCLUDES(_mutex);

    void
    GotRouterContact(const RouterID& router, const RouterContact& rc) EXCLUDES(_mutex);

    void
    InvalidRouter(const RouterID& router);

    void
    RouterNotFound(const RouterID& router);

    void
    OnRouterContactResult(
        const RouterID& router, const RouterContact* const rc, const RCRequestResult result);

    void
    VerifyRC(const RouterContact rc);

    void
    CreatePendingSession(const RouterID& router) EXCLUDES(_mutex);

    void
    FinalizeRequest(const RouterID& router, const SessionResult type) EXCLUDES(_mutex);

    mutable util::Mutex _mutex;  // protects pendingSessions, pendingCallbacks

    std::unordered_map<RouterID, std::shared_ptr<PendingSession>> pendingSessions
        GUARDED_BY(_mutex);

    std::unordered_map<RouterID, CallbacksQueue> pendingCallbacks GUARDED_BY(_mutex);

    AbstractRouter* _router = nullptr;
    ILinkManager* _linkManager = nullptr;
    I_RCLookupHandler* _rcLookup = nullptr;
    Profiling* _profiler = nullptr;
    std::shared_ptr<NodeDB> _nodedb;
    EventLoop_ptr _loop;
    WorkerFunc_t work;
    RouterID us;
  };

}  // namespace llarp
