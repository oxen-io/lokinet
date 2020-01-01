#include <llarp.hpp>
#include <dht/context.hpp>
#include <dht/key.hpp>
#include <router/abstractrouter.hpp>
#include <link/session.hpp>
#include <util/thread/logic.hpp>
#include <signal.h>

static llarp_main* ctx = nullptr;

int
main(int argc, char* argv[])
{
  (void)argc;
  (void)argv;
  static auto sighandler = [](int sig) {
    if(ctx == nullptr)
      return;
    llarp_main_signal(ctx, sig);
  };
  signal(SIGINT, sighandler);
  signal(SIGTERM, sighandler);
  signal(SIGHUP, sighandler);
  llarp::SetLogLevel(llarp::eLogInfo);
  ctx = llarp_main_spider_init();
  if(ctx == nullptr)
    return 1;
  if(llarp_main_setup(ctx))
    return 1;

  std::thread spider([]() {
    do
    {
      if(llarp_main_is_running(ctx))
      {
        auto ctx_pp = llarp::Context::Get(ctx);
        LogicCall(ctx_pp->logic,
                  [r = ctx_pp->router.get(), dht = ctx_pp->router->dht()]() {
                    r->ForEachPeer(
                        [dht](const llarp::ILinkSession* s, bool) {
                          if(s == nullptr || not s->IsEstablished())
                            return;
                          const llarp::dht::Key_t k(s->GetPubKey().data());
                          dht->impl->ExploreNetworkVia(k);
                        },
                        true);
                  });
      }
      std::this_thread::sleep_for(std::chrono::seconds(1));
    } while(llarp_main_is_running(ctx));
  });

  auto retcode = llarp_main_run(ctx, llarp_main_runtime_opts{});
  spider.join();
  llarp_main_free(ctx);
  return retcode;
}
