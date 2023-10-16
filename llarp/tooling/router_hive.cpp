#include "router_hive.hpp"

#include <llarp.hpp>
#include <llarp/util/str.hpp>
#include <llarp/router/router.hpp>

#include <chrono>
#include <algorithm>
#include <csignal>

using namespace std::chrono_literals;

namespace tooling
{
  void
  RouterHive::AddRouter(const std::shared_ptr<llarp::Config>& config, bool isSNode)
  {
    auto& container = (isSNode ? relays : clients);

    llarp::RuntimeOptions opts;
    opts.isSNode = isSNode;

    Context_ptr context = std::make_shared<HiveContext>(this);
    context->Configure(config);
    context->Setup(opts);

    auto routerId = llarp::RouterID(context->router->pubkey());
    container[routerId] = context;
    fmt::print("Generated router with ID {}\n", routerId);
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

    for (const auto& [routerId, ctx] : container)
    {
      routerMainThreads.emplace_back([ctx = ctx, isRelay = isRelay]() {
        ctx->Run(llarp::RuntimeOptions{false, false, isRelay});
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
    for (auto& [routerId, ctx] : relays)
    {
      ctx->loop->call([ctx = ctx]() { ctx->HandleSignal(SIGINT); });
    }
    for (auto& [routerId, ctx] : clients)
    {
      ctx->loop->call([ctx = ctx]() { ctx->HandleSignal(SIGINT); });
    }

    llarp::LogInfo("Waiting on routers to be stopped");
    for (auto [routerId, ctx] : relays)
    {
      while (ctx->IsUp())
      {
        std::this_thread::sleep_for(10ms);
      }
    }
    for (auto [routerId, ctx] : clients)
    {
      while (ctx->IsUp())
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
  RouterHive::VisitRouter(Context_ptr ctx, std::function<void(Context_ptr)> visit)
  {
    // TODO: this should be called from each router's appropriate Loop, e.g.:
    //     ctx->loop->call([visit, ctx]() { visit(ctx); });
    // however, this causes visit calls to be deferred
    visit(ctx);
  }

  HiveRouter*
  RouterHive::GetRelay(const llarp::RouterID& id, bool needMutexLock)
  {
    auto guard =
        needMutexLock ? std::make_optional<std::lock_guard<std::mutex>>(routerMutex) : std::nullopt;

    auto itr = relays.find(id);
    if (itr == relays.end())
      return nullptr;

    auto ctx = itr->second;
    return ctx->getRouterAsHiveRouter();
  }

  std::vector<size_t>
  RouterHive::RelayConnectedRelays()
  {
    std::lock_guard guard{routerMutex};
    std::vector<size_t> results;
    results.resize(relays.size());
    std::mutex results_lock;

    size_t i = 0;
    size_t done_count = 0;
    for (auto& [routerId, ctx] : relays)
    {
      ctx->loop->call([&, i, ctx = ctx]() {
        size_t count = ctx->router->NumberOfConnectedRouters();
        std::lock_guard guard{results_lock};
        results[i] = count;
        done_count++;
      });
      i++;
    }

    while (true)
    {
      size_t read_done_count = 0;
      {
        std::lock_guard guard{results_lock};
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
    std::lock_guard<std::mutex> guard{routerMutex};
    std::vector<llarp::RouterContact> results;
    results.resize(relays.size());

    size_t i = 0;
    for (auto [routerId, ctx] : relays)
    {
      results[i] = ctx->router->rc();
      i++;
    }
    return results;
  }

  void
  RouterHive::ForEachRelay(std::function<void(Context_ptr)> visit)
  {
    for (auto [routerId, ctx] : relays)
    {
      VisitRouter(ctx, visit);
    }
  }

  void
  RouterHive::ForEachClient(std::function<void(Context_ptr)> visit)
  {
    for (auto [routerId, ctx] : clients)
    {
      VisitRouter(ctx, visit);
    }
  }

  /// safely visit every router context
  void
  RouterHive::ForEachRouter(std::function<void(Context_ptr)> visit)
  {
    ForEachRelay(visit);
    ForEachClient(visit);
  }

}  // namespace tooling
