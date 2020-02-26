#pragma once

#include <tooling/router_event.hpp>

#include <util/thread/queue.hpp>

namespace tooling
{

  struct RouterHive
  {
    constexpr size_t MAX_EVENT_QUEUE_SIZE = 200;

    RouterHive(size_t eventQueueSize = MAX_EVENT_QUEUE_SIZE);

    void
    InformEvent(RouterEvent event);

    void
    ProcessEventQueue();


    /*
     * Event processing function declarations
     */

    void
    ProcessPathBuildAttempt(PathBuildAttemptEvent event);



    llarp::thread::Queue<RouterEvent> eventQueue;
  };

} // namespace tooling
