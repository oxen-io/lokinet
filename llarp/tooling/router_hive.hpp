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
}

namespace tooling
{
  struct RouterHive
  {
    using Context_ptr = std::shared_ptr<llarp::Context>;

   private:
    void
    StartRouters(std::vector<llarp_main*>* routers);

    void
    AddRouter(
        const std::shared_ptr<llarp::Config>& config,
        std::vector<llarp_main*>* routers,
        bool isRelay);

    /// safely visit router
    void
    VisitRouter(llarp_main* router, std::function<void(Context_ptr)> visit);

    /// safely visit relay at index N
    void
    VisitRelay(size_t index, std::function<void(Context_ptr)> visit);

    /// safely visit client at index N
    void
    VisitClient(size_t index, std::function<void(Context_ptr)> visit);

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

    void
    ForEachRelay(std::function<void(Context_ptr)> visit)
    {
      for (size_t idx = 0; idx < relays.size(); ++idx)
      {
        VisitRelay(idx, visit);
      }
    }

    void
    ForEachClient(std::function<void(Context_ptr)> visit)
    {
      for (size_t idx = 0; idx < clients.size(); ++idx)
      {
        VisitClient(idx, visit);
      }
    }

    /// safely visit every router context
    void
    ForEachRouter(std::function<void(Context_ptr)> visit)
    {
      ForEachRelay(visit);
      ForEachClient(visit);
    }

    std::vector<size_t>
    RelayConnectedRelays();

    std::vector<llarp::RouterContact>
    GetRelayRCs();

    std::vector<llarp_main*> relays;
    std::vector<llarp_main*> clients;

    std::vector<std::thread> routerMainThreads;

    std::mutex eventQueueMutex;
    std::deque<RouterEventPtr> eventQueue;
  };

}  // namespace tooling
