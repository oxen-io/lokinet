#include <llarp.hpp>
#include <dht/context.hpp>
#include <dht/key.hpp>
#include <router/abstractrouter.hpp>
#include <link/session.hpp>
#include <util/thread/logic.hpp>
#include <signal.h>
#include <util/logging/logger.hpp>
#include <util/logging/null_logger.hpp>

#include <future>
static llarp_main* ctx = nullptr;

int
main(int argc, char* argv[])
{
  std::vector< llarp::RouterID > targets;
  if(argc == 1)
    return 1;
  for(int idx = 1; idx < argc; idx++)
  {
    targets.emplace_back();
    auto& target = targets.back();
    if(not target.FromString(argv[idx]))
    {
      llarp::PubKey k;
      if(not k.FromString(argv[idx]))
        return 1;
      target = k;
    }
    std::cout << "lookup " << target << std::endl;
  }

  static auto sighandler = [](int sig) {
    if(ctx == nullptr)
      return;
    llarp_main_signal(ctx, sig);
  };
  signal(SIGINT, sighandler);
  signal(SIGTERM, sighandler);
  signal(SIGHUP, sighandler);

  llarp::LogContext::Instance().logStream =
      std::make_unique< llarp::NullLogStream >();

  ctx = llarp_main_spider_init();
  if(ctx == nullptr)
    return 1;
  if(llarp_main_setup(ctx))
    return 1;
  std::thread lookup_thread([targets]() {
    do
    {
      if(llarp_main_is_running(ctx))
      {
        auto do_lookup = [](llarp::RouterID target) -> bool {
          std::promise< absl::optional< llarp::RouterContact > > found;
          auto ctx_pp = llarp::Context::Get(ctx);
          auto ftr    = found.get_future();
          LogicCall(
              ctx_pp->logic, [dht = ctx_pp->router->dht(), target, &found]() {
                dht->impl->LookupRouter(
                    target,
                    [&found](
                        const std::vector< llarp::RouterContact >& results) {
                      if(results.size() == 0)
                      {
                        found.set_value(
                            absl::optional< llarp::RouterContact >{});
                      }
                      else
                      {
                        found.set_value(results[0]);
                      }
                    });
              });
          auto result = ftr.get();
          if(result.has_value())
          {
            std::cout << "found " << target << " " << result.value()
                      << std::endl;
            return true;
          }
          else
          {
            return false;
          }
        };
        for(const auto& target : targets)
        {
          if(not do_lookup(target))
          {
            std::cout << target << " not found" << std::endl;
          }
        }
        llarp_main_signal(ctx, SIGTERM);
        return;
      }
      else
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while(true);
  });

  auto retcode = llarp_main_run(ctx, llarp_main_runtime_opts{});
  lookup_thread.join();
  llarp_main_free(ctx);
  return retcode;
}
