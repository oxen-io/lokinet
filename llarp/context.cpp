#include <getopt.h>
#include <llarp.h>
#include <llarp/logger.h>
#include <signal.h>
#include <sys/param.h>  // for MIN
#include <llarp.hpp>
#include "router.hpp"

#include <llarp/dnsd.hpp>
#include <llarp/dns_dotlokilookup.hpp>

#if(__FreeBSD__) || (__OpenBSD__) || (__NetBSD__)
#include <pthread_np.h>
#endif

namespace llarp
{
  Context::~Context()
  {
    llarp_ev_loop_free(&mainloop);
  }

  void
  Context::progress()
  {
    std::cout << "." << std::flush;
  }

  bool
  Context::ReloadConfig()
  {
    // llarp::LogInfo("loading config at ", configfile);
    if(llarp_load_config(config, configfile.c_str()))
    {
      llarp_free_config(&config);
      llarp::LogError("failed to load config file ", configfile);
      return false;
    }
    llarp_config_iterator iter;
    iter.user  = this;
    iter.visit = &iter_config;
    llarp_config_iter(config, &iter);
    return router->ReloadConfig(config);
  }

  void
  Context::iter_config(llarp_config_iterator *itr, const char *section,
                       const char *key, const char *val)
  {
    Context *ctx = static_cast< Context * >(itr->user);
    if(!strcmp(section, "router"))
    {
      if(!strcmp(key, "worker-threads") && !ctx->singleThreaded)
      {
        int workers = atoi(val);
        if(workers > 0 && ctx->worker == nullptr)
        {
          ctx->worker = llarp_init_threadpool(workers, "llarp-worker");
        }
      }
      else if(!strcmp(key, "net-threads"))
      {
        ctx->num_nethreads = atoi(val);
        if(ctx->num_nethreads <= 0)
          ctx->num_nethreads = 1;
        if(ctx->singleThreaded)
          ctx->num_nethreads = 0;
      }
    }
    if(!strcmp(section, "netdb"))
    {
      if(!strcmp(key, "dir"))
      {
        ctx->nodedb_dir = val;
      }
    }
  }

  int
  Context::LoadDatabase()
  {
    llarp_crypto_init(&crypto);
    nodedb = llarp_nodedb_new(&crypto);

    if(!llarp_nodedb_ensure_dir(nodedb_dir.c_str()))
    {
      llarp::LogError("nodedb_dir is incorrect");
      return 0;
    }
    // llarp::LogInfo("nodedb_dir [", nodedb_dir, "] configured!");
    ssize_t loaded = llarp_nodedb_load_dir(nodedb, nodedb_dir.c_str());
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

  int
  Context::IterateDatabase(struct llarp_nodedb_iter i)
  {
    return llarp_nodedb_iterate_all(nodedb, i);
  }

  bool
  Context::PutDatabase(struct llarp::RouterContact &rc)
  {
    // FIXME
    // return llarp_nodedb_put_rc(nodedb, rc);
    return false;
  }

  llarp::RouterContact *
  Context::GetDatabase(const byte_t *pk)
  {
    // FIXME
    // return llarp_nodedb_get_rc(nodedb, pk);
    return nullptr;
  }

  int
  Context::Setup()
  {
    llarp::LogInfo(LLARP_VERSION, " ", LLARP_RELEASE_MOTTO);
    llarp::LogInfo("starting up");
    if(!this->LoadDatabase())
      return -1;
    llarp_ev_loop_alloc(&mainloop);

    // ensure worker thread pool
    if(!worker && !singleThreaded)
      worker = llarp_init_threadpool(2, "llarp-worker");
    else if(singleThreaded)
    {
      llarp::LogInfo("running in single threaded mode");
      worker = llarp_init_same_process_threadpool();
    }
    // ensure netio thread
    if(singleThreaded)
    {
      logic = llarp_init_single_process_logic(worker);
    }
    else
      logic = llarp_init_logic();

    router = llarp_init_router(worker, mainloop, logic);

    if(!llarp_configure_router(router, config))
    {
      llarp::LogError("Failed to configure router");
      return 1;
    }
    // set nodedb, load our RC, establish DHT
    llarp_run_router(router, nodedb);

    return 0;  // success
  }

  int
  Context::Run()
  {
    // just check to make sure it's not already set up (either this or we add a
    // bool and/or add another function)
    if(!this->router)
    {
      // set up all requirements
      if(this->Setup())
      {
        llarp::LogError("Failed to setup router");
        return 1;
      }
    }

    // run net io thread
    llarp::LogInfo("running mainloop");
    llarp_ev_loop_run_single_process(mainloop, worker, logic);
    return 0;
  }

  void
  Context::HandleSignal(int sig)
  {
    if(sig == SIGINT)
    {
      llarp::LogInfo("SIGINT");
      SigINT();
    }
    // TODO(despair): implement hot-reloading config on NT
#ifndef _WIN32
    if(sig == SIGHUP)
    {
      llarp::LogInfo("SIGHUP");
      ReloadConfig();
    }
#endif
  }

  void
  Context::SigINT()
  {
    Close();
  }

  void
  Context::Close()
  {
    llarp::LogDebug("stop router");
    if(router)
      llarp_stop_router(router);

    llarp::LogDebug("stop workers");
    if(worker)
      llarp_threadpool_stop(worker);

    llarp::LogDebug("join workers");
    if(worker)
      llarp_threadpool_join(worker);

    llarp::LogDebug("stop logic");

    if(logic)
      llarp_logic_stop(logic);

    llarp::LogDebug("free config");
    llarp_free_config(&config);

    llarp::LogDebug("free workers");
    llarp_free_threadpool(&worker);

    llarp::LogDebug("free nodedb");
    llarp_nodedb_free(&nodedb);

    llarp::LogDebug("stopping event loop");
    llarp_ev_loop_stop(mainloop);

    llarp::LogDebug("free router");
    llarp_free_router(&router);

    llarp::LogDebug("free logic");
    llarp_free_logic(&logic);

    for(auto &t : netio_threads)
    {
      llarp::LogDebug("join netio thread");
      t.join();
    }

    netio_threads.clear();
  }

  bool
  Context::LoadConfig(const std::string &fname)
  {
    llarp_new_config(&config);
    configfile = fname;
    return ReloadConfig();
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
    llarp_main *m = new llarp_main;
    m->ctx.reset(new llarp::Context());
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
  llarp_main_setup(struct llarp_main *ptr)
  {
    return ptr->ctx->Setup();
  }

  int
  llarp_main_run(struct llarp_main *ptr)
  {
    if(!ptr)
    {
      llarp::LogError("No ptr passed in");
      return 0;
    }
    return ptr->ctx->Run();
  }

  void
  llarp_main_abort(struct llarp_main *ptr)
  {
    llarp_logic_stop_timer(ptr->ctx->router->logic);
  }

  void
  llarp_main_queryDHT_RC(struct llarp_main *ptr,
                         struct llarp_router_lookup_job *job)
  {
    llarp_dht_lookup_router(ptr->ctx->router->dht, job);
  }

  bool
  llarp_main_init_dnsd(struct llarp_main *ptr, struct dnsd_context *dnsd,
                       const llarp::Addr &dnsd_sockaddr,
                       const llarp::Addr &dnsc_sockaddr)
  {
    return llarp_dnsd_init(dnsd, ptr->ctx->logic, ptr->ctx->mainloop,
                           dnsd_sockaddr, dnsc_sockaddr);
  }

  bool
  llarp_main_init_dotLokiLookup(struct llarp_main *ptr,
                                struct dotLokiLookup *dll)
  {
    dll->logic = ptr->ctx->logic;
    return true;
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

  int
  llarp_main_iterateDatabase(struct llarp_main *ptr, struct llarp_nodedb_iter i)
  {
    return ptr->ctx->IterateDatabase(i);
  }

  bool
  llarp_main_putDatabase(struct llarp_main *ptr, llarp::RouterContact &rc)
  {
    return ptr->ctx->PutDatabase(rc);
  }

  llarp::RouterContact *
  llarp_main_getDatabase(struct llarp_main *ptr, byte_t *pk)
  {
    return ptr->ctx->GetDatabase(pk);
  }

  llarp::RouterContact *
  llarp_main_getLocalRC(struct llarp_main *ptr)
  {
    //
    /*
     llarp_config_iterator iter;
     iter.user  = this;
     iter.visit = &iter_config;
     llarp_config_iter(ctx->config, &iter);
     */
    // llarp_rc *rc = new llarp_rc;
    // llarp::RouterContact *rc = new llarp::RouterContact;
    // llarp_rc_new(rc);
    // llarp::LogInfo("FIXME: Loading ", ptr->ctx->conatctFile);
    // FIXME
    /*
    if(llarp_rc_read(ptr->ctx->conatctFile, rc))
      return rc;
    else
    */
    return nullptr;
  }

  void
  llarp_main_checkOnline(void *u, uint64_t orig, uint64_t left)
  {
    // llarp::Info("checkOnline - check ", left);
    if(left)
      return;
    struct check_online_request *request =
        static_cast< struct check_online_request * >(u);
    // llarp::Debug("checkOnline - running");
    // llarp::Info("checkOnline - DHT nodes ",
    // request->ptr->ctx->router->dht->impl.nodes->nodes.size());
    request->online = false;
    request->nodes  = request->ptr->ctx->router->dht->impl.nodes->nodes.size();
    if(request->ptr->ctx->router->dht->impl.nodes->nodes.size())
    {
      // llarp::Info("checkOnline - Going to say we're online");
      request->online = true;
    }
    request->hook(request);
    // reschedue our self
    llarp_main_queryDHT(request);
  }

  void
  llarp_main_queryDHT_online(struct check_online_request *request)
  {
    // Info("llarp_main_queryDHT_online: ", request->online ? "online" :
    // "offline");
    if(request->online && !request->first)
    {
      request->first = true;
      llarp::LogInfo("llarp_main_queryDHT_online - We're online");
      llarp::LogInfo("llarp_main_queryDHT_online - Querying DHT");
      llarp_dht_lookup_router(request->ptr->ctx->router->dht, request->job);
    }
  }

  void
  llarp_main_queryDHT(struct check_online_request *request)
  {
    // llarp::Info("llarp_main_queryDHT - setting up timer");
    request->hook = &llarp_main_queryDHT_online;
    llarp_logic_call_later(request->ptr->ctx->router->logic,
                           {1000, request, &llarp_main_checkOnline});
    // llarp_dht_lookup_router(ptr->ctx->router->dht, job);
  }

  bool
  main_router_mapAddress(struct llarp_main *ptr,
                         const llarp::service::Address &addr, uint32_t ip)
  {
    auto *endpoint = &ptr->ctx->router->hiddenServiceContext;
    return endpoint->MapAddress(addr, llarp::huint32_t{ip});
  }

  bool
  main_router_prefetch(struct llarp_main *ptr,
                       const llarp::service::Address &addr)
  {
    auto *endpoint = &ptr->ctx->router->hiddenServiceContext;
    return endpoint->Prefetch(addr);
  }

  llarp::handlers::TunEndpoint *
  main_router_getFirstTunEndpoint(struct llarp_main *ptr)
  {
    auto *context = &ptr->ctx->router->hiddenServiceContext;
    return context->getFirstTun();
  }

  //#include <llarp/service/context.hpp>
  bool
  main_router_endpoint_iterator(
      struct llarp_main *ptr, struct llarp::service::Context::endpoint_iter &i)
  {
    auto *context = &ptr->ctx->router->hiddenServiceContext;
    return context->iterate(i);
  }

  llarp_tun_io *
  main_router_getRange(struct llarp_main *ptr)
  {
    auto *context = &ptr->ctx->router->hiddenServiceContext;
    return context->getRange();
  }

  const char *
  handleBaseCmdLineArgs(int argc, char *argv[])
  {
    const char *conffname = "daemon.ini";
    int c;
    while(1)
    {
      static struct option long_options[] = {
          {"config", required_argument, 0, 'c'},
          {"logLevel", required_argument, 0, 'o'},
          {0, 0, 0, 0}};
      int option_index = 0;
      c = getopt_long(argc, argv, "c:o:", long_options, &option_index);
      if(c == -1)
        break;
      switch(c)
      {
        case 0:
          break;
        case 'c':
          conffname = optarg;
          break;
        case 'o':
          if(strncmp(optarg, "debug", std::min(strlen(optarg), size_t(5))) == 0)
          {
            cSetLogLevel(eLogDebug);
          }
          else if(strncmp(optarg, "info", std::min(strlen(optarg), size_t(4)))
                  == 0)
          {
            cSetLogLevel(eLogInfo);
          }
          else if(strncmp(optarg, "warn", std::min(strlen(optarg), size_t(4)))
                  == 0)
          {
            cSetLogLevel(eLogWarn);
          }
          else if(strncmp(optarg, "error", std::min(strlen(optarg), size_t(5)))
                  == 0)
          {
            cSetLogLevel(eLogError);
          }
          break;
        default:
          break;
      }
    }
    return conffname;
  }
}
