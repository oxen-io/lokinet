#include <llarp.hpp>
#include <llarp.h>

#include <config.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <crypto/crypto_noop.hpp>
#include <dht/context.hpp>
#include <dnsd.hpp>
#include <ev/ev.hpp>
#include <metrics/metrictank_publisher.hpp>
#include <metrics/json_publisher.hpp>
#include <metrics/stream_publisher.hpp>
#include <nodedb.hpp>
#include <router/router.hpp>
#include <util/logger.h>
#include <util/memfn.hpp>
#include <util/metrics.hpp>
#include <util/scheduler.hpp>

#include <absl/strings/str_split.h>
#include <cxxopts.hpp>
#include <signal.h>

#if(__FreeBSD__) || (__OpenBSD__) || (__NetBSD__)
#include <pthread_np.h>
#endif

namespace llarp
{
  Context::Context()
  {
  }

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
    config->visit(util::memFn(&Context::iter_config, this));

    if(!disableMetrics)
    {
      setupMetrics();
      if(!disableMetricLogs)
      {
        m_metricsManager->instance()->addGlobalPublisher(
            std::make_shared< metrics::StreamPublisher >(std::cerr));
      }
    }
    return true;
  }

  void
  Context::iter_config(const char *section, const char *key, const char *val)
  {
    if(!strcmp(section, "system"))
    {
      if(!strcmp(key, "pidfile"))
      {
        SetPIDFile(val);
      }
    }
    if(!strcmp(section, "metrics"))
    {
      if(!strcmp(key, "disable-metrics"))
      {
        disableMetrics = true;
      }
      else if(!strcmp(key, "disable-metrics-log"))
      {
        disableMetricLogs = true;
      }
      else if(!strcmp(key, "json-metrics-path"))
      {
        jsonMetricsPath = val;
      }
      else if(!strcmp(key, "metric-tank-host"))
      {
        metricTankHost = val;
      }
      else
      {
        // consume everything else as a metric tag
        metricTags[key] = val;
      }
    }
    if(!strcmp(section, "router"))
    {
      if(!strcmp(key, "worker-threads") && !singleThreaded)
      {
        int workers = atoi(val);
        if(workers > 0 && worker == nullptr)
        {
          worker.reset(llarp_init_threadpool(workers, "llarp-worker"));
        }
      }
      else if(!strcmp(key, "net-threads"))
      {
        num_nethreads = atoi(val);
        if(num_nethreads <= 0)
          num_nethreads = 1;
        if(singleThreaded)
          num_nethreads = 0;
      }
    }
    if(!strcmp(section, "netdb"))
    {
      if(!strcmp(key, "dir"))
      {
        nodedb_dir = val;
      }
    }
  }

  void
  Context::setupMetrics()
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

    if(!jsonMetricsPath.native().empty())
    {
      m_metricsManager->instance()->addGlobalPublisher(
          std::make_shared< metrics::JsonPublisher >(
              std::bind(&metrics::JsonPublisher::directoryPublisher,
                        std::placeholders::_1, jsonMetricsPath)));
    }

    if(!metricTankHost.empty())
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
            absl::StrSplit(metricTankHost, ':');

        m_metricsManager->instance()->addGlobalPublisher(
            std::make_shared< metrics::MetricTankPublisher >(
                metricTags, split.first, stoi(split.second)));
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
    nodedb = std::make_unique< llarp_nodedb >(router->diskworker());

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
    // ensure worker thread pool
    if(!worker && !singleThreaded)
      worker.reset(llarp_init_threadpool(2, "llarp-worker"));
    else if(singleThreaded)
    {
      llarp::LogInfo("running in single threaded mode");
      worker.reset(llarp_init_same_process_threadpool());
    }
    // ensure netio thread
    if(singleThreaded)
    {
      logic = std::make_shared< Logic >(worker.get());
    }
    else
      logic = std::make_shared< Logic >();

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

    router = std::make_unique< Router >(worker.get(), mainloop, logic);
    if(!router->Configure(config.get()))
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
    llarp_ev_loop_run_single_process(mainloop, worker.get(), logic);
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
    else
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
      llarp_threadpool_stop(worker.get());

    llarp::LogDebug("join workers");
    if(worker)
      llarp_threadpool_join(worker.get());

    llarp::LogDebug("free config");
    config.release();

    llarp::LogDebug("free workers");
    worker.release();

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

extern "C"
{
  struct llarp_main *
  llarp_main_init(const char *fname, bool multiProcess)
  {
    if(!fname)
      fname = "daemon.ini";
    char *var = getenv("LLARP_DEBUG");
    if(var && *var == '1')
    {
      cSetLogLevel(eLogDebug);
    }
    llarp_main *m          = new llarp_main;
    m->ctx                 = std::make_unique< llarp::Context >();
    m->ctx->singleThreaded = !multiProcess;
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
