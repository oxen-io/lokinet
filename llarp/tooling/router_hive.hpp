#pragma once

#include "router_event.hpp"

#include <llarp.hpp>
#include <config/config.hpp>
#include <tooling/hive_context.hpp>

#include <vector>
#include <deque>
#include <thread>
#include <mutex>

struct llarp_config;
struct llarp_main;

namespace llarp
{
  struct Context;
}  // namespace llarp

namespace tooling
{
  struct HiveRouter;  // Hive's version of Router

  struct RouterHive
  {
    using Context_ptr = std::shared_ptr<HiveContext>;

   private:
    void
    StartRouters(bool isRelay);

    void
    AddRouter(const std::shared_ptr<llarp::Config>& config, bool isRelay);

    /// safely visit router (asynchronously)
    void
    VisitRouter(Context_ptr ctx, std::function<void(Context_ptr)> visit);

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

    // functions to safely visit each relay and/or client's HiveContext
    void
    ForEachRelay(std::function<void(Context_ptr)> visit);
    void
    ForEachClient(std::function<void(Context_ptr)> visit);
    void
    ForEachRouter(std::function<void(Context_ptr)> visit);

    HiveRouter*
    GetRelay(const llarp::RouterID& id, bool needMutexLock = true);

    std::vector<size_t>
    RelayConnectedRelays();

    std::vector<llarp::RouterContact>
    GetRelayRCs();

    std::mutex routerMutex;
    std::unordered_map<llarp::RouterID, Context_ptr> relays;
    std::unordered_map<llarp::RouterID, Context_ptr> clients;

    std::vector<std::thread> routerMainThreads;

    std::mutex eventQueueMutex;
    std::deque<RouterEventPtr> eventQueue;
  };

}  // namespace tooling
