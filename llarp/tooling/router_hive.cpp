#include <tooling/router_hive.hpp>

#include "llarp.h"
#include "llarp.hpp"
#include "util/thread/logic.hpp"
#include "util/str.hpp"
#include "router/abstractrouter.hpp"

#include <chrono>
#include <algorithm>

using namespace std::chrono_literals;

namespace tooling
{
  void
  RouterHive::AddRouter(const std::shared_ptr<llarp::Config>& config, bool isRelay)
  {
    auto& container = (isRelay ? relays : clients);

    llarp_main* ctx = llarp_main_init_from_config(config->Copy(), isRelay);
    auto result = llarp_main_setup(ctx, isRelay);
    if (result == 0)
    {
      auto context = llarp::Context::Get(ctx);
      auto routerId = llarp::RouterID(context->router->pubkey());
      context->InjectHive(this);
      container[routerId] = ctx;
      std::cout << "Generated router with ID " << routerId << std::endl;
    }
    else
    {
      throw std::runtime_error(llarp::stringify(
          "Failed to add RouterHive ",
          (isRelay ? "relay" : "client"),
          ", llarp_main_setup() returned ",
          result));
    }
  }

  void
  RouterHive::AddRelay(const std::shared_ptr<llarp::Config>& config)
  {
    AddRouter(config, true);
  }

  void
  RouterHive::AddClient(const std::shared_ptr<llarp::Config>& config)
  {
    AddRouter(config, false);
  }

  void
  RouterHive::StartRouters(bool isRelay)
  {
    auto& container = (isRelay ? relays : clients);

    for (auto [routerId, ctx] : container)
    {
      routerMainThreads.emplace_back([=]() {
        llarp_main_run(ctx, llarp_main_runtime_opts{false, false, false, isRelay});
      });
      std::this_thread::sleep_for(2ms);
    }
  }

  void
  RouterHive::StartRelays()
  {
    StartRouters(true);
  }

  void
  RouterHive::StartClients()
  {
    StartRouters(false);
  }

  void
  RouterHive::StopRouters()
  {
    llarp::LogInfo("Signalling all routers to stop");
    for (auto [routerId, ctx] : relays)
    {
      llarp_main_signal(ctx, 2 /* SIGINT */);
    }
    for (auto [routerId, ctx] : clients)
    {
      llarp_main_signal(ctx, 2 /* SIGINT */);
    }

    llarp::LogInfo("Waiting on routers to be stopped");
    for (auto [routerId, ctx] : relays)
    {
      while (llarp_main_is_running(ctx))
      {
        std::this_thread::sleep_for(10ms);
      }
    }
    for (auto [routerId, ctx] : clients)
    {
      while (llarp_main_is_running(ctx))
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
  RouterHive::VisitRouter(llarp_main* router, std::function<void(Context_ptr)> visit)
  {
    auto ctx = llarp::Context::Get(router);
    LogicCall(ctx->logic, [visit, ctx]() { visit(ctx); });
  }

  std::vector<size_t>
  RouterHive::RelayConnectedRelays()
  {
    std::vector<size_t> results;
    results.resize(relays.size());
    std::mutex results_lock;

    size_t i = 0;
    size_t done_count = 0;
    for (auto [routerId, ctx] : relays)
    {
      auto context = llarp::Context::Get(ctx);
      LogicCall(context->logic, [&, i, context]() {
        size_t count = context->router->NumberOfConnectedRouters();
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

  // TODO: DRY -- this smells a lot like  RelayConnectedRelays()
  std::vector<llarp::RouterContact>
  RouterHive::GetRelayRCs()
  {
    std::vector<llarp::RouterContact> results;
    results.resize(relays.size());

    size_t i = 0;
    for (auto [routerId, ctx] : relays)
    {
      auto context = llarp::Context::Get(ctx);
      results[i] = context->router->rc();
      i++;
    }
    return results;
  }

}  // namespace tooling
