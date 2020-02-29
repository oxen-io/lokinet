#pragma once

#include <tooling/router_event.hpp>

#include <llarp.h>
#include <config/config.hpp>

#include <vector>
#include <queue>
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
    RouterHive() = default;

    void
    AddRouter(const std::shared_ptr<llarp::Config> & conf);

    void
    StartRouters();

    void
    StopRouters();

    void
    NotifyEvent(RouterEventPtr event);

    RouterEventPtr
    GetNextEvent();


    using Context_ptr = std::shared_ptr<llarp::Context>;

    /// safely visit every router context
    void 
    ForEachRouter(std::function<void(Context_ptr)> visit)
    {
      for(size_t idx = 0; idx < routers.size(); ++idx)
      {
        VisitRouter(idx, visit);
      }
    }

    /// safely visit router at index N
    void
    VisitRouter(size_t index, std::function<void(Context_ptr)> visit);


    std::vector<llarp_main *> routers;

    std::vector<std::thread> routerMainThreads;

    std::mutex eventQueueMutex;
    std::queue<RouterEventPtr> eventQueue;
  };

} // namespace tooling
