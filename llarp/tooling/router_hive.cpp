#include <tooling/router_hive.hpp>

#include "llarp.h"
#include "llarp.hpp"
#include "util/thread/logic.hpp"

#include <chrono>

using namespace std::chrono_literals;

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
    for (llarp_main* ctx : routers)
    {
      routerMainThreads.emplace_back([ctx](){
            llarp_main_run(ctx, llarp_main_runtime_opts{false, false, false});
      });
      std::this_thread::sleep_for(20ms);
    }
  }

  void
  RouterHive::StopRouters()
  {

llarp::LogWarn("Signalling all routers to stop");
    for (llarp_main* ctx : routers)
    {
      llarp_main_signal(ctx, 2 /* SIGINT */);
    }

size_t i=0;
llarp::LogWarn("Waiting on routers to be stopped");
    for (llarp_main* ctx : routers)
    {
      while(llarp_main_is_running(ctx))
      {
llarp::LogWarn("Waiting on router ", i, " to stop");
        std::this_thread::sleep_for(10ms);
      }
i++;
    }

//llarp::LogWarn("Joining router threads");
//i=0;
    for (auto& thread : routerMainThreads)
    {
//llarp::LogWarn("Attempting to join router thread ", i);
      while (not thread.joinable())
      {
//llarp::LogWarn("Waiting on router thread ", i, " to be joinable");
        std::this_thread::sleep_for(500ms);
      }
      thread.join();
//llarp::LogWarn("Joined router thread ", i);
//i++;
    }

llarp::LogWarn("RouterHive::StopRouters finished");
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

  RouterEventPtr
  RouterHive::GetNextEvent()
  {
    auto ptr = eventQueue.popFrontWithTimeout(50ms);
    if (ptr)
    {
      return std::move(ptr.value());
    }
    return nullptr;
  }

  void
  RouterHive::ProcessPathBuildAttempt(const PathBuildAttemptEvent& event)
  {
  }


  void
  RouterHive::VisitRouter(size_t index, std::function<void(Context_ptr)> visit)
  {
    if(index >= routers.size())
    {
      visit(nullptr);
      return;
    }
    auto * r = routers[index];
    auto ctx = llarp::Context::Get(r);
    LogicCall(ctx->logic, [visit, ctx]() {
      visit(ctx);
    });
  }

} // namespace tooling
