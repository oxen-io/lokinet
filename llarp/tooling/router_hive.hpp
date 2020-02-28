#pragma once

#include <tooling/router_event.hpp>

#include <llarp.h>
#include <config/config.hpp>
#include <util/thread/queue.hpp>

#include <vector>
#include <thread>

struct llarp_config;
struct llarp_main;

namespace tooling
{

  struct RouterHive
  {
    static const size_t MAX_EVENT_QUEUE_SIZE;

    RouterHive(size_t eventQueueSize = MAX_EVENT_QUEUE_SIZE);

    void
    AddRouter(const std::shared_ptr<llarp::Config> & conf);

    void
    StartRouters();

    void
    StopRouters();

    void
    NotifyEvent(RouterEventPtr event);

    void
    ProcessEventQueue();

    RouterEventPtr
    GetNextEvent();


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
