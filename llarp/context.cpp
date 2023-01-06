#include <llarp.hpp>
#include "constants/version.hpp"
#include "constants/evloop.hpp"

#include "config/config.hpp"
#include "crypto/crypto_libsodium.hpp"
#include "dht/context.hpp"
#include "ev/ev.hpp"
#include <memory>
#include "nodedb.hpp"
#include "router/router.hpp"
#include "service/context.hpp"
#include "util/logging.hpp"

#include <llarp/util/service_manager.hpp>

#include <CLI/App.hpp>
#include <CLI/Formatter.hpp>
#include <CLI/Config.hpp>

#include <csignal>
#include <stdexcept>

#if (__FreeBSD__) || (__OpenBSD__) || (__NetBSD__)
#include <pthread_np.h>
#endif

static auto logcat = llarp::log::Cat("llarp-context");

namespace llarp
{
  bool
  Context::CallSafe(std::function<void(void)> f)
  {
    if (!loop)
      return false;
    loop->call_soon(std::move(f));
    return true;
  }

  void
  Context::Configure(std::shared_ptr<Config> conf)
  {
    if (nullptr != config.get())
      throw std::runtime_error("Config already exists");

    config = std::move(conf);

    nodedb_dir = fs::path{config->router.m_dataDir / nodedb_dirname}.string();
  }

  bool
  Context::IsUp() const
  {
    return router && router->IsRunning();
  }

  bool
  Context::LooksAlive() const
  {
    return router && router->LooksAlive();
  }

  void
  Context::Setup(const RuntimeOptions& opts)
  {
    /// Call one of the Configure() methods before calling Setup()
    if (not config)
      throw std::runtime_error("Cannot call Setup() on context without a Config");

    if (opts.showBanner)
      llarp::LogInfo(fmt::format("{} {}", llarp::VERSION_FULL, llarp::RELEASE_MOTTO));

    if (!loop)
    {
      auto jobQueueSize = std::max(event_loop_queue_size, config->router.m_JobQueueSize);
      loop = EventLoop::create(jobQueueSize);
    }

    crypto = std::make_shared<sodium::CryptoLibSodium>();
    cryptoManager = std::make_shared<CryptoManager>(crypto.get());

    router = makeRouter(loop);

    nodedb = makeNodeDB();

    if (!router->Configure(config, opts.isSNode, nodedb))
      throw std::runtime_error("Failed to configure router");
  }

  std::shared_ptr<NodeDB>
  Context::makeNodeDB()
  {
    return std::make_shared<NodeDB>(
        nodedb_dir, [r = router.get()](auto call) { r->QueueDiskIO(std::move(call)); });
  }

  std::shared_ptr<AbstractRouter>
  Context::makeRouter(const EventLoop_ptr& loop)
  {
    return std::static_pointer_cast<AbstractRouter>(
        std::make_shared<Router>(loop, makeVPNPlatform()));
  }

  std::shared_ptr<vpn::Platform>
  Context::makeVPNPlatform()
  {
    auto plat = vpn::MakeNativePlatform(this);
    if (plat == nullptr)
      throw std::runtime_error("vpn platform not supported");
    return plat;
  }

  int
  Context::Run(const RuntimeOptions&)
  {
    if (router == nullptr)
    {
      // we are not set up so we should die
      llarp::LogError("cannot run non configured context");
      return 1;
    }

    if (not router->Run())
      return 2;

    // run net io thread
    llarp::LogInfo("running mainloop");

    loop->run();
    if (closeWaiter)
    {
      closeWaiter->set_value();
    }
    Close();
    return 0;
  }

  void
  Context::CloseAsync()
  {
    /// already closing
    if (IsStopping())
      return;

    loop->call([this]() { HandleSignal(SIGTERM); });
    closeWaiter = std::make_unique<std::promise<void>>();
  }

  bool
  Context::IsStopping() const
  {
    return closeWaiter.operator bool();
  }

  void
  Context::Wait()
  {
    if (closeWaiter)
    {
      closeWaiter->get_future().wait();
      closeWaiter.reset();
    }
  }

  void
  Context::HandleSignal(int sig)
  {
    llarp::log::debug(logcat, "Handling signal {}", sig);
    if (sig == SIGINT || sig == SIGTERM)
    {
      SigINT();
    }
#ifndef _WIN32
    if (sig == SIGUSR1)
    {
      if (router and not router->IsServiceNode())
      {
        LogInfo("SIGUSR1: resetting network state");
        router->Thaw();
      }
    }
    if (sig == SIGHUP)
    {
      Reload();
    }
#endif
  }

  void
  Context::Reload()
  {}

  void
  Context::SigINT()
  {
    if (router)
    {
      llarp::log::debug(logcat, "Handling SIGINT");
      /// async stop router on sigint
      router->Stop();
    }
  }

  void
  Context::Close()
  {
    llarp::LogDebug("free config");
    config.reset();

    llarp::LogDebug("free nodedb");
    nodedb.reset();

    llarp::LogDebug("free router");
    router.reset();

    llarp::LogDebug("free loop");
    loop.reset();
  }

  Context::Context()
  {
    // service_manager is a global and context isnt
    llarp::sys::service_manager->give_context(this);
  }

}  // namespace llarp
