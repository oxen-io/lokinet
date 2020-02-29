#include <tooling/router_hive.hpp>

#include "llarp.h"
#include "llarp.hpp"
#include "util/thread/logic.hpp"

#include <chrono>

using namespace std::chrono_literals;

namespace tooling
{

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

    llarp::LogInfo("Signalling all routers to stop");
    for (llarp_main* ctx : routers)
    {
      llarp_main_signal(ctx, 2 /* SIGINT */);
    }

    llarp::LogInfo("Waiting on routers to be stopped");
    for (llarp_main* ctx : routers)
    {
      while(llarp_main_is_running(ctx))
      {
        std::this_thread::sleep_for(10ms);
      }
    }

    llarp::LogInfo("Joining all router threads");
    for (auto& thread : routerMainThreads)
    {
      while (not thread.joinable())
      {
        std::this_thread::sleep_for(500ms);
      }
      thread.join();
    }

    llarp::LogInfo("RouterHive::StopRouters finished");
  }

  void
  RouterHive::NotifyEvent(RouterEventPtr event)
  {
    std::lock_guard<std::mutex> guard{eventQueueMutex};

    eventQueue.push(std::move(event));
  }

  RouterEventPtr
  RouterHive::GetNextEvent()
  {
    std::lock_guard<std::mutex> guard{eventQueueMutex};

    if (not eventQueue.empty())
    {
      auto ptr = std::move(eventQueue.front());
      eventQueue.pop();
      return ptr;
    }
    return nullptr;
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
