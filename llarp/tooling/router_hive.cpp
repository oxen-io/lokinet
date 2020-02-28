#include <tooling/router_hive.hpp>

#include "llarp.h"
#include "llarp.hpp"

#include <chrono>

namespace tooling
{

  const size_t RouterHive::MAX_EVENT_QUEUE_SIZE = 200;

  RouterHive::RouterHive(size_t eventQueueSize) : eventQueue(eventQueueSize)
  {
  }

  void
  RouterHive::AddRouter(const std::shared_ptr<llarp::Config> & config)
  {
    llarp_main* ctx = llarp_main_init_from_config(config->Copy());
    if(llarp_main_setup(ctx) == 0)
    {
      llarp::Context::Get(ctx)->InjectHive(this);
      routers.push_back(ctx);
    }
  }

  void
  RouterHive::StartRouters()
  {
    llarp_main_runtime_opts opts{false,false,false};

    for (llarp_main* ctx : routers)
    {
      routerMainThreads.emplace_back([=](){ llarp_main_run(ctx, opts); });
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

    for (auto& thread : routerMainThreads)
    {
      while (not thread.joinable())
      {
        llarp::LogWarn("Waiting for router thread to be joinable");
        std::this_thread::sleep_for(500ms);
      }
      thread.join();
    }
  }

  void
  RouterHive::NotifyEvent(RouterEventPtr event)
  {
    if(eventQueue.tryPushBack(std::move(event))
       != llarp::thread::QueueReturn::Success)
    {
      llarp::LogError("RouterHive Event Queue appears to be full.  Either implement/change time dilation or increase the queue size.");
    }
  }

  void
  RouterHive::ProcessEventQueue()
  {
    while(not eventQueue.empty())
    {
      RouterEventPtr event = eventQueue.popFront();

      event->Process(*this);
    }
  }

  void
  RouterHive::ProcessPathBuildAttempt(const PathBuildAttemptEvent& event)
  {
  }


} // namespace tooling
