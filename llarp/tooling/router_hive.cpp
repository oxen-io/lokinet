#include <tooling/router_hive.hpp>

#include "llarp.h"
#include "llarp.hpp"
#include "util/thread/logic.hpp"
#include "router/abstractrouter.hpp"

#include <chrono>
#include <algorithm>

using namespace std::chrono_literals;

namespace tooling
{
  void
  RouterHive::AddRouter(const std::shared_ptr<llarp::Config> & config, std::vector<llarp_main *> *routers)
  {
    llarp_main* ctx = llarp_main_init_from_config(config->Copy());
    if(llarp_main_setup(ctx) == 0)
    {
      llarp::Context::Get(ctx)->InjectHive(this);
      routers->push_back(ctx);
    }
  }

  void
  RouterHive::AddRelay(const std::shared_ptr<llarp::Config> & config)
  {
    AddRouter(config, &relays);
  }

  void
  RouterHive::AddClient(const std::shared_ptr<llarp::Config> & config)
  {
    AddRouter(config, &clients);
  }

  void
  RouterHive::StartRouters(std::vector<llarp_main *> *routers)
  {
    for (llarp_main* ctx : *routers)
    {
      routerMainThreads.emplace_back([ctx](){
            llarp_main_run(ctx, llarp_main_runtime_opts{false, false, false});
      });
      std::this_thread::sleep_for(2ms);
    }
  }

  void
  RouterHive::StartRelays()
  {
    StartRouters(&relays);
  }

  void
  RouterHive::StartClients()
  {
    StartRouters(&clients);
  }

  void
  RouterHive::StopRouters()
  {

    llarp::LogInfo("Signalling all routers to stop");
    for (llarp_main* ctx : relays)
    {
      llarp_main_signal(ctx, 2 /* SIGINT */);
    }
    for (llarp_main* ctx : clients)
    {
      llarp_main_signal(ctx, 2 /* SIGINT */);
    }

    llarp::LogInfo("Waiting on routers to be stopped");
    for (llarp_main* ctx : relays)
    {
      while(llarp_main_is_running(ctx))
      {
        std::this_thread::sleep_for(10ms);
      }
    }
    for (llarp_main* ctx : clients)
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

    eventQueue.push_back(std::move(event));
  }

  RouterEventPtr
  RouterHive::GetNextEvent()
  {
    std::lock_guard<std::mutex> guard{eventQueueMutex};

    if (not eventQueue.empty())
    {
      auto ptr = std::move(eventQueue.front());
      eventQueue.pop_front();
      return ptr;
    }
    return nullptr;
  }

  std::deque<RouterEventPtr>
  RouterHive::GetAllEvents()
  {
    std::lock_guard<std::mutex> guard{eventQueueMutex};

    std::deque<RouterEventPtr> events;
    if (not eventQueue.empty())
    {
      eventQueue.swap(events);
    }
    return events;
  }

  void
  RouterHive::VisitRouter(llarp_main *router, std::function<void(Context_ptr)> visit)
  {
    auto ctx = llarp::Context::Get(router);
    LogicCall(ctx->logic, [visit, ctx]() {
      visit(ctx);
    });
  }

  void
  RouterHive::VisitRelay(size_t index, std::function<void(Context_ptr)> visit)
  {
    if(index >= relays.size())
    {
      visit(nullptr);
      return;
    }
    VisitRouter(relays[index], visit);
  }

  void
  RouterHive::VisitClient(size_t index, std::function<void(Context_ptr)> visit)
  {
    if(index >= clients.size())
    {
      visit(nullptr);
      return;
    }
    VisitRouter(clients[index], visit);
  }

  std::vector<size_t>
  RouterHive::RelayConnectedRelays()
  {
    std::vector<size_t> results;
    results.resize(relays.size());
    std::mutex results_lock;

    size_t i=0;
    size_t done_count = 0;
    for (auto relay : relays)
    {
      auto ctx = llarp::Context::Get(relay);
      LogicCall(ctx->logic, [&, i, ctx]() {
        size_t count = ctx->router->NumberOfConnectedRouters();
        std::lock_guard<std::mutex> guard{results_lock};
        results[i] = count;
        done_count++;
      });
      i++;
    }

    while (true)
    {
      size_t read_done_count = 0;
      {
        std::lock_guard<std::mutex> guard{results_lock};
        read_done_count = done_count;
      }
      if (read_done_count == relays.size())
        break;

      std::this_thread::sleep_for(100ms);
    }
    return results;
  }

  std::vector<llarp::RouterContact>
  RouterHive::GetRelayRCs()
  {
    std::vector<llarp::RouterContact> results;
    results.resize(relays.size());
    std::mutex results_lock;

    size_t i=0;
    size_t done_count = 0;
    for (auto relay : relays)
    {
      auto ctx = llarp::Context::Get(relay);
      LogicCall(ctx->logic, [&, i, ctx]() {
        llarp::RouterContact rc = ctx->router->rc();
        std::lock_guard<std::mutex> guard{results_lock};
        results[i] = std::move(rc);
        done_count++;
      });
      i++;
    }

    while (true)
    {
      size_t read_done_count = 0;
      {
        std::lock_guard<std::mutex> guard{results_lock};
        read_done_count = done_count;
      }
      if (read_done_count == relays.size())
        break;

      std::this_thread::sleep_for(100ms);
    }
    return results;
  }

} // namespace tooling
