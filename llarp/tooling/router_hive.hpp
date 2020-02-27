#pragma once

#include <tooling/router_event.hpp>

#include <llarp.h>
#include <util/thread/queue.hpp>

#include <vector>
#include <thread>

struct llarp_config;
struct llarp_main;

namespace tooling
{

  struct RouterHive
  {
    constexpr size_t MAX_EVENT_QUEUE_SIZE = 200;

    RouterHive(size_t eventQueueSize = MAX_EVENT_QUEUE_SIZE);

    void
    AddRouter(llarp_config* conf);

    void
    StartRouters();

    void
    StopRouters();

    void
    NotifyEvent(RouterEvent event);

    void
    ProcessEventQueue();


    /*
     * Event processing function declarations
     */

    void
    ProcessPathBuildAttempt(PathBuildAttemptEvent event);


    std::vector<llarp_main *> routers;

    std::vector<std::thread> routerMainThreads;

    llarp::thread::Queue<RouterEvent> eventQueue;
  };

} // namespace tooling
