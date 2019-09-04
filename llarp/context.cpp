#include <llarp.hpp>
#include <llarp.h>

#include <config/config.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <crypto/crypto_noop.hpp>
#include <dht/context.hpp>
#include <dnsd.hpp>
#include <ev/ev.hpp>
#include <nodedb.hpp>
#include <router/router.hpp>
#include <util/logging/logger.h>
#include <util/meta/memfn.hpp>
#include <util/metrics/json_publisher.hpp>
#include <util/metrics/metrics.hpp>
#include <util/metrics/metrictank_publisher.hpp>
#include <util/metrics/stream_publisher.hpp>
#include <util/thread/scheduler.hpp>

#include <absl/strings/str_split.h>
#include <cxxopts.hpp>
#include <csignal>

#if(__FreeBSD__) || (__OpenBSD__) || (__NetBSD__)
#include <pthread_np.h>
#endif

namespace llarp
{
  Context::Context() = default;

  Context::~Context()
  {
    m_scheduler->stop();
  }

  void
  Context::progress()
  {
    std::cout << "." << std::flush;
  }

  bool
  Context::Configure()
  {
    // llarp::LogInfo("loading config at ", configfile);
    if(!config->Load(configfile.c_str()))
    {
      config.release();
      llarp::LogError("failed to load config file ", configfile);
      return false;
    }

    // System config
    if(!config->system.pidfile.empty())
    {
      SetPIDFile(config->system.pidfile);
    }
    auto threads = config->router.workerThreads();
    if(threads <= 0)
      threads = 1;
    worker = std::make_shared< llarp::thread::ThreadPool >(threads, 1024,
                                                           "llarp-worker");

    nodedb_dir = config->netdb.nodedbDir();

    if(!config->metrics.disableMetrics)
    {
      auto &metricsConfig = config->metrics;
      auto &tags          = metricsConfig.metricTags;
      tags["netid"]       = config->router.netId();
      tags["nickname"]    = config->router.nickname();
      setupMetrics(metricsConfig);
      if(!config->metrics.disableMetricLogs)
      {
        m_metricsManager->instance()->addGlobalPublisher(
            std::make_shared< metrics::StreamPublisher >(std::cerr));
      }
    }
    return true;
  }

  void
  Context::setupMetrics(const MetricsConfig &metricsConfig)
  {
    if(!m_scheduler)
    {
      m_scheduler = std::make_unique< thread::Scheduler >();
    }
    if(!m_metricsManager)
    {
      m_metricsManager = std::make_unique< metrics::DefaultManagerGuard >();
    }
    if(!m_metricsPublisher)
    {
      m_metricsPublisher = std::make_unique< metrics::PublisherScheduler >(
          *m_scheduler, m_metricsManager->instance());
    }

    if(!metricsConfig.jsonMetricsPath.native().empty())
    {
      m_metricsManager->instance()->addGlobalPublisher(
          std::make_shared< metrics::JsonPublisher >(
              std::bind(&metrics::JsonPublisher::directoryPublisher,
                        std::placeholders::_1, metricsConfig.jsonMetricsPath)));
    }

    if(!metricsConfig.metricTankHost.empty())
    {
      if(std::getenv("LOKINET_ENABLE_METRIC_TANK"))
      {
        static std::string WARNING = R"(
__        ___    ____  _   _ ___ _   _  ____
\ \      / / \  |  _ \| \ | |_ _| \ | |/ ___|
 \ \ /\ / / _ \ | |_) |  \| || ||  \| | |  _
  \ V  V / ___ \|  _ <| |\  || || |\  | |_| |
   \_/\_/_/   \_\_| \_\_| \_|___|_| \_|\____|

This Lokinet session is not private!!

Sending connection metrics to metrictank!!
__        ___    ____  _   _ ___ _   _  ____
\ \      / / \  |  _ \| \ | |_ _| \ | |/ ___|
 \ \ /\ / / _ \ | |_) |  \| || ||  \| | |  _
  \ V  V / ___ \|  _ <| |\  || || |\  | |_| |
   \_/\_/_/   \_\_| \_\_| \_|___|_| \_|\____|

        )";

        std::cerr << WARNING << '\n';

        std::pair< std::string, std::string > split =
            absl::StrSplit(metricsConfig.metricTankHost, ':');

        m_metricsManager->instance()->addGlobalPublisher(
            std::make_shared< metrics::MetricTankPublisher >(
                metricsConfig.metricTags, split.first, stoi(split.second)));
      }
      else
      {
        std::cerr << "metrictank host specified, but "
                     "LOKINET_ENABLE_METRIC_TANK not set, skipping\n";
      }
    }

    m_metricsPublisher->setDefault(absl::Seconds(30));

    m_scheduler->start();
  }

  void
  Context::SetPIDFile(const std::string &fname)
  {
    pidfile = fname;
  }

  int
  Context::LoadDatabase()
  {
    if(!llarp_nodedb::ensure_dir(nodedb_dir.c_str()))
    {
      llarp::LogError("nodedb_dir is incorrect");
      return 0;
    }
    // llarp::LogInfo("nodedb_dir [", nodedb_dir, "] configured!");
    ssize_t loaded = nodedb->load_dir(nodedb_dir.c_str());
    llarp::LogInfo("nodedb_dir loaded ", loaded, " RCs from [", nodedb_dir,
                   "]");
    if(loaded < 0)
    {
      // shouldn't be possible
      llarp::LogError("nodedb_dir directory doesn't exist");
      return 0;
    }
    return 1;
  }

  bool
  Context::PutDatabase(__attribute__((unused)) struct llarp::RouterContact &rc)
  {
    // FIXME
    // return llarp_nodedb_put_rc(nodedb, rc);
    return false;
  }

  llarp::RouterContact *
  Context::GetDatabase(__attribute__((unused)) const byte_t *pk)
  {
    // FIXME
    // return llarp_nodedb_get_rc(nodedb, pk);
    return nullptr;
  }

  int
  Context::Setup(bool debug)
  {
    llarp::LogInfo(LLARP_VERSION, " ", LLARP_RELEASE_MOTTO);
    llarp::LogInfo("starting up");
    mainloop = llarp_make_ev_loop();
    logic    = std::make_shared< Logic >();

    if(debug)
    {
      static std::string WARNING = R"(
__        ___    ____  _   _ ___ _   _  ____
\ \      / / \  |  _ \| \ | |_ _| \ | |/ ___|
 \ \ /\ / / _ \ | |_) |  \| || ||  \| | |  _
  \ V  V / ___ \|  _ <| |\  || || |\  | |_| |
   \_/\_/_/   \_\_| \_\_| \_|___|_| \_|\____|

This Lokinet session is not private!!

Sending traffic unencrypted!!
__        ___    ____  _   _ ___ _   _  ____
\ \      / / \  |  _ \| \ | |_ _| \ | |/ ___|
 \ \ /\ / / _ \ | |_) |  \| || ||  \| | |  _
  \ V  V / ___ \|  _ <| |\  || || |\  | |_| |
   \_/\_/_/   \_\_| \_\_| \_|___|_| \_|\____|

        )";

      std::cerr << WARNING << '\n';
      crypto = std::make_unique< NoOpCrypto >();
    }
    else
    {
      crypto = std::make_unique< sodium::CryptoLibSodium >();
    }
    cryptoManager = std::make_unique< CryptoManager >(crypto.get());

    router = std::make_unique< Router >(worker, mainloop, logic);

    nodedb = std::make_unique< llarp_nodedb >(router->diskworker());

    if(!router->Configure(config.get(), nodedb.get()))
    {
      llarp::LogError("Failed to configure router");
      return 1;
    }

    // must be done after router is made so we can use its disk io worker
    // must also be done after configure so that netid is properly set if it
    // is provided by config
    if(!this->LoadDatabase())
      return 1;

    return 0;
  }

  int
  Context::Run()
  {
    if(router == nullptr)
    {
      // we are not set up so we should die
      llarp::LogError("cannot run non configured context");
      return 1;
    }
    if(!WritePIDFile())
      return 1;
    // run
    if(!router->Run(nodedb.get()))
      return 1;

    // run net io thread
    llarp::LogInfo("running mainloop");
    llarp_ev_loop_run_single_process(mainloop, logic);
    // waits for router graceful stop
    return 0;
  }

  bool
  Context::WritePIDFile() const
  {
    if(pidfile.size())
    {
      std::ofstream f(pidfile);
      f << std::to_string(getpid());
      return f.good();
    }

    return true;
  }

  void
  Context::RemovePIDFile() const
  {
    if(pidfile.size())
    {
      fs::path f = pidfile;
      std::error_code ex;
      if(fs::exists(f, ex))
      {
        if(!ex)
          fs::remove(f);
      }
    }
  }

  void
  Context::HandleSignal(int sig)
  {
    if(sig == SIGINT || sig == SIGTERM)
    {
      SigINT();
    }
    // TODO(despair): implement hot-reloading config on NT
#ifndef _WIN32
    if(sig == SIGHUP)
    {
      llarp::LogInfo("SIGHUP");
      if(router)
      {
        router->hiddenServiceContext().ForEachService(
            [](const std::string &name,
               const llarp::service::Endpoint_ptr &ep) -> bool {
              ep->ResetInternalState();
              llarp::LogInfo("Reset internal state for ", name);
              return true;
            });
        router->PumpLL();
        Config newconfig;
        if(!newconfig.Load(configfile.c_str()))
        {
          llarp::LogError("failed to load config file ", configfile);
          return;
        }
        // validate config
        if(!router->ValidateConfig(&newconfig))
        {
          llarp::LogWarn("new configuration is invalid");
          return;
        }
        // reconfigure
        if(!router->Reconfigure(&newconfig))
        {
          llarp::LogError("Failed to reconfigure so we will stop.");
          router->Stop();
          return;
        }
        llarp::LogInfo("router reconfigured");
      }
    }
#endif
  }

  void
  Context::SigINT()
  {
    if(router)
    {
      /// async stop router on sigint
      router->Stop();
    }
    else
    {
      if(logic)
        logic->stop();
      llarp_ev_loop_stop(mainloop);
      Close();
    }
  }

  void
  Context::Close()
  {
    llarp::LogDebug("stop workers");
    if(worker)
      worker->stop();

    llarp::LogDebug("free config");
    config.release();

    llarp::LogDebug("free workers");
    worker.reset();

    llarp::LogDebug("free nodedb");
    nodedb.release();

    llarp::LogDebug("free router");
    router.release();

    llarp::LogDebug("free logic");
    logic.reset();

    RemovePIDFile();
  }

  bool
  Context::LoadConfig(const std::string &fname)
  {
    config     = std::make_unique< Config >();
    configfile = fname;
    return Configure();
  }
}  // namespace llarp

struct llarp_main
{
  std::unique_ptr< llarp::Context > ctx;
};

llarp::Context *
llarp_main_get_context(llarp_main *m)
{
  return m->ctx.get();
}

extern "C"
{
  struct llarp_main *
  llarp_main_init(const char *fname, bool multiProcess)
  {
    (void)multiProcess;
    if(!fname)
      fname = "daemon.ini";
    char *var = getenv("LLARP_DEBUG");
    if(var && *var == '1')
    {
      cSetLogLevel(eLogDebug);
    }
    auto *m = new llarp_main;
    m->ctx  = std::make_unique< llarp::Context >();
    if(!m->ctx->LoadConfig(fname))
    {
      m->ctx->Close();
      delete m;
      return nullptr;
    }
    return m;
  }

  void
  llarp_main_signal(struct llarp_main *ptr, int sig)
  {
    ptr->ctx->HandleSignal(sig);
  }

  int
  llarp_main_setup(struct llarp_main *ptr, bool debug)
  {
    return ptr->ctx->Setup(debug);
  }

  int
  llarp_main_run(struct llarp_main *ptr)
  {
    if(!ptr)
    {
      llarp::LogError("No ptr passed in");
      return 1;
    }
    return ptr->ctx->Run();
  }

  void
  llarp_main_abort(struct llarp_main *ptr)
  {
    ptr->ctx->router->logic()->stop_timer();
  }

  void
  llarp_main_queryDHT_RC(struct llarp_main *ptr,
                         struct llarp_router_lookup_job *job)
  {
    llarp_dht_lookup_router(ptr->ctx->router->dht(), job);
  }

  bool
  llarp_main_init_dnsd(struct llarp_main *ptr, struct dnsd_context *dnsd,
                       const llarp::Addr &dnsd_sockaddr,
                       const llarp::Addr &dnsc_sockaddr)
  {
    return llarp_dnsd_init(dnsd, ptr->ctx->logic.get(),
                           ptr->ctx->mainloop.get(), dnsd_sockaddr,
                           dnsc_sockaddr);
  }

  bool
  llarp_main_init_dotLokiLookup(struct llarp_main *ptr,
                                struct dotLokiLookup *dll)
  {
    (void)ptr;
    (void)dll;
    // TODO: gut me
    return false;
  }

  void
  llarp_main_free(struct llarp_main *ptr)
  {
    delete ptr;
  }

  int
  llarp_main_loadDatabase(struct llarp_main *ptr)
  {
    return ptr->ctx->LoadDatabase();
  }

  const char *
  handleBaseCmdLineArgs(int argc, char *argv[])
  {
    // clang-format off
    cxxopts::Options options(
		"lokinet",
		"Lokinet is a private, decentralized and IP based overlay network for the internet"
    );
    options.add_options()
		("c,config", "Config file", cxxopts::value< std::string >()->default_value("daemon.ini"))
		("o,logLevel", "logging level");
    // clang-format on

    auto result          = options.parse(argc, argv);
    std::string logLevel = result["logLevel"].as< std::string >();

    if(logLevel == "debug")
    {
      cSetLogLevel(eLogDebug);
    }
    else if(logLevel == "info")
    {
      cSetLogLevel(eLogInfo);
    }
    else if(logLevel == "warn")
    {
      cSetLogLevel(eLogWarn);
    }
    else if(logLevel == "error")
    {
      cSetLogLevel(eLogError);
    }

    // this isn't thread safe, but reconfiguring during run is likely unsafe
    // either way
    static std::string confname = result["config"].as< std::string >();

    return confname.c_str();
  }
}
