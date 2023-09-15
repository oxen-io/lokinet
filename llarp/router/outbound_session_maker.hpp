#pragma once

/*
    Note:
      - this class is marked for destruction
      - functionality to be absorbed into llarp/link/link_manager.hpp
        - can be subdivided into multiple objects

*/

#include "rc_lookup_handler.hpp"

#include <llarp/util/thread/threading.hpp>
#include <llarp/util/types.hpp>
#include <llarp/util/formattable.hpp>
#include <llarp/profiling.hpp>

#include <unordered_map>
#include <list>
#include <memory>

namespace llarp
{
  struct PendingSession;

  struct LinkManager;
  struct I_RCLookupHandler;

  enum class SessionResult
  {
    Establish,
    Timeout,
    RouterNotFound,
    InvalidRouter,
    NoLink,
    EstablishFail
  };

  constexpr std::string_view
  ToString(SessionResult sr)
  {
    return sr == llarp::SessionResult::Establish     ? "success"sv
        : sr == llarp::SessionResult::Timeout        ? "timeout"sv
        : sr == llarp::SessionResult::NoLink         ? "no link"sv
        : sr == llarp::SessionResult::InvalidRouter  ? "invalid router"sv
        : sr == llarp::SessionResult::RouterNotFound ? "not found"sv
        : sr == llarp::SessionResult::EstablishFail  ? "establish failed"sv
                                                     : "???"sv;
  }
  template <>
  constexpr inline bool IsToStringFormattable<SessionResult> = true;

  using RouterCallback = std::function<void(const RouterID&, const SessionResult)>;

  struct OutboundSessionMaker
  {
    using Work_t = std::function<void(void)>;
    using WorkerFunc_t = std::function<void(Work_t)>;

    using CallbacksQueue = std::list<RouterCallback>;

   public:
    ~OutboundSessionMaker() = default;

    bool
    OnSessionEstablished(AbstractLinkSession* session);

    void
    OnConnectTimeout(AbstractLinkSession* session);

    void
    CreateSessionTo(const RouterID& router, RouterCallback on_result) EXCLUDES(_mutex);

    void
    CreateSessionTo(const RouterContact& rc, RouterCallback on_result) EXCLUDES(_mutex);

    bool
    HavePendingSessionTo(const RouterID& router) const EXCLUDES(_mutex);

    void
    ConnectToRandomRouters(int numDesired);

    util::StatusObject
    ExtractStatus() const;

    bool
    ShouldConnectTo(const RouterID& router) const EXCLUDES(_mutex);

    void
    Init(
        Router* router,
        LinkManager* linkManager,
        RCLookupHandler* rcLookup,
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

    mutable util::Mutex _mutex;  // protects pendingCallbacks

    std::unordered_map<RouterID, CallbacksQueue> pendingCallbacks GUARDED_BY(_mutex);

    Router* _router = nullptr;
    LinkManager* _linkManager = nullptr;
    RCLookupHandler* _rcLookup = nullptr;
    Profiling* _profiler = nullptr;
    std::shared_ptr<NodeDB> _nodedb;
    EventLoop_ptr _loop;
    WorkerFunc_t work;
    RouterID us;
  };

}  // namespace llarp
