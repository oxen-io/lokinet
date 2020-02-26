#include <tooling/router_hive.hpp>

#include <chrono>

namespace tooling
{

  RouterHive::RouterHive(size_t eventQueueSize) : eventQueue(eventQueueSize)
  {
  }

  void
  RouterHive::AddRouter(llarp_config* conf)
  {
    llarp_main* ctx = llarp_main_init_from_config(conf);
    routers.push_back(ctx);
  }

  void
  RouterHive::StartRouters()
  {
    llarp_main_runtime_opts opts{false,false,false};

    for (llarp_main* ctx : routers)
    {
      routerMainThreads.emplace_back({std::bind(&llarp_main_run, ctx, opts)});
    }
  }

  void
  RouterHive::StopRouters()
  {
    using namespace std::chrono_literals;

    for (llarp_main* ctx : routers)
    {
      llarp_main_signal(ctx, 2 /* SIGINT */);
    }

    for (llarp_main* ctx : routers)
    {
      while(llarp_main_is_running(ctx))
      {
        std::this_thread::sleep_for(10ms);
      }
    }
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
