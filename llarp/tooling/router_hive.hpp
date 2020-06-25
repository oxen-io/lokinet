#pragma once

#include <tooling/router_event.hpp>

#include <llarp.h>
#include <config/config.hpp>

#include <vector>
#include <deque>
#include <thread>
#include <mutex>

struct llarp_config;
struct llarp_main;

namespace llarp
{
  struct Context;
  struct AbstractRouter;
}  // namespace llarp

namespace tooling
{
  struct RouterHive
  {
    using Context_ptr = std::shared_ptr<llarp::Context>;

   private:
    void
    StartRouters(bool isRelay);

    void
    AddRouter(const std::shared_ptr<llarp::Config>& config, bool isRelay);

    /// safely visit router
    void
    VisitRouter(llarp_main* router, std::function<void(Context_ptr)> visit);

   public:
    RouterHive() = default;

    void
    AddRelay(const std::shared_ptr<llarp::Config>& conf);

    void
    AddClient(const std::shared_ptr<llarp::Config>& conf);

    void
    StartRelays();

    void
    StartClients();

    void
    StopRouters();

    void
    NotifyEvent(RouterEventPtr event);

    RouterEventPtr
    GetNextEvent();

    std::deque<RouterEventPtr>
    GetAllEvents();

    // functions to safely visit each relay and/or client's AbstractRouter or Context
    void
    ForEachRelayRouter(std::function<void(llarp::AbstractRouter*)> visit);
    void
    ForEachClientRouter(std::function<void(llarp::AbstractRouter*)> visit);
    void
    ForEachRouterRouter(std::function<void(llarp::AbstractRouter*)> visit);

    void
    ForEachRelayContext(std::function<void(Context_ptr)> visit);
    void
    ForEachClientContext(std::function<void(Context_ptr)> visit);
    void
    ForEachRouterContext(std::function<void(Context_ptr)> visit);

    llarp::AbstractRouter*
    GetRelay(const llarp::RouterID& id, bool needMutexLock = true);

    std::vector<size_t>
    RelayConnectedRelays();

    std::vector<llarp::RouterContact>
    GetRelayRCs();

    std::mutex routerMutex;
    std::unordered_map<llarp::RouterID, llarp_main*, llarp::RouterID::Hash> relays;
    std::unordered_map<llarp::RouterID, llarp_main*, llarp::RouterID::Hash> clients;

    std::vector<std::thread> routerMainThreads;

    std::mutex eventQueueMutex;
    std::deque<RouterEventPtr> eventQueue;
  };

}  // namespace tooling
