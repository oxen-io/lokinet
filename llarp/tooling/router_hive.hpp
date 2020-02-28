#pragma once

#include <tooling/router_event.hpp>

#include <llarp.h>
#include <config/config.hpp>
#include <util/thread/queue.hpp>

#include <vector>
#include <thread>

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
    static const size_t MAX_EVENT_QUEUE_SIZE;

    RouterHive(size_t eventQueueSize = MAX_EVENT_QUEUE_SIZE);

    void
    AddRouter(const std::shared_ptr<llarp::Config> & conf);

    void
    StartRouters(std::function<void(llarp_main*)> runit);

    void
    StopRouters();

    void
    NotifyEvent(RouterEventPtr event);

    void
    ProcessEventQueue();

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

    /*
     * Event processing function declarations
     */

    void
    ProcessPathBuildAttempt(const PathBuildAttemptEvent& event);


    std::vector<llarp_main *> routers;

    std::vector<std::thread> routerMainThreads;

    llarp::thread::Queue<RouterEventPtr> eventQueue;
  };

} // namespace tooling
