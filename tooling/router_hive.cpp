#include <tooling/router_hive.hpp>

namespace tooling
{

  RouterHive::RouterHive(size_t eventQueueSize) : eventQueue(eventQueueSize)
  {
  }

  void
  RouterHive::InformEvent(RouterEvent event)
  {
    if(eventQueue.tryPushBack(std::move(event))
       != llarp::thread::QueueReturn::Success)
    {
      LogError("RouterHive Event Queue appears to be full.  Either implement/change time dilation or increase the queue size.");
    }
  }

  void
  RouterHive::ProcessEventQueue()
  {
    while(not eventQueue.empty())
    {
      RouterEvent event = eventQueue.popFront();

      event.Process(*this);
    }
  }



  void
  ProcessPathBuildAttempt(PathBuildAttemptEvent event)
  {
  }


} // namespace tooling
