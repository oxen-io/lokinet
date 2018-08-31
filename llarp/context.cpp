#include <getopt.h>
#include <llarp.h>
#include <llarp/logger.h>
#include <signal.h>
#ifndef _MSC_VER
#include <sys/param.h>  // for MIN
#endif
#include <llarp.hpp>
#include "logger.hpp"
#include "math.h"
#include "router.hpp"

#if(__FreeBSD__) || (__OpenBSD__) || (__NetBSD__)
#include <pthread_np.h>
#endif

// keep this once jeff reenables concurrency
#ifdef _MSC_VER
extern "C" void
SetThreadName(DWORD dwThreadID, LPCSTR szThreadName);
#endif

#if _WIN32 || __sun
#define wmin(x, y) (((x) < (y)) ? (x) : (y))
#define MIN wmin
#endif

namespace llarp
{
  Context::~Context()
  {
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
    llarp::LogInfo("config [", configfile, "] loaded");
    return true;
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
      else if(!strcmp(key, "contact-file"))
      {
        strncpy(ctx->conatctFile, val, fmin(255, strlen(val)));
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
        strncpy(ctx->nodedb_dir, val, sizeof(ctx->nodedb_dir));
      }
    }
  }

  int
  Context::LoadDatabase()
  {
    llarp_crypto_libsodium_init(&crypto);
    nodedb = llarp_nodedb_new(&crypto);
    if(!nodedb_dir[0])
    {
      llarp::LogError("no nodedb_dir configured");
      return 0;
    }

    nodedb_dir[sizeof(nodedb_dir) - 1] = 0;
    if(!llarp_nodedb_ensure_dir(nodedb_dir))
    {
      llarp::LogError("nodedb_dir is incorrect");
      return 0;
    }
    // llarp::LogInfo("nodedb_dir [", nodedb_dir, "] configured!");
    ssize_t loaded = llarp_nodedb_load_dir(nodedb, nodedb_dir);
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
  Context::Setup()
  {
    llarp::LogInfo(LLARP_VERSION, " ", LLARP_RELEASE_MOTTO);
    llarp::LogInfo("starting up");
    this->LoadDatabase();
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
    if(singleThreaded)
    {
      llarp::LogInfo("running mainloop");
      llarp_ev_loop_run_single_process(mainloop, worker, logic);
    }
    else
    {
      llarp::LogInfo("running mainloop");
      return llarp_ev_loop_run(mainloop, logic);
    }
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

    for(size_t i = 0; i < netio_threads.size(); ++i)
    {
      if(mainloop)
      {
        llarp::LogDebug("stopping event loop thread ", i);
        llarp_ev_loop_stop(mainloop);
      }
    }

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
    llarp::LogDebug("free mainloop");
    llarp_ev_loop_free(&mainloop);
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
    return ptr->ctx->Run();
  }

  void
  llarp_main_abort(struct llarp_main *ptr)
  {
    llarp_logic_stop_timer(ptr->ctx->router->logic);
  }

  void
  llarp_main_free(struct llarp_main *ptr)
  {
    delete ptr;
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
          if(strncmp(optarg, "debug", MIN(strlen(optarg), (unsigned long)5))
             == 0)
          {
            cSetLogLevel(eLogDebug);
          }
          else if(strncmp(optarg, "info", MIN(strlen(optarg), (unsigned long)4))
                  == 0)
          {
            cSetLogLevel(eLogInfo);
          }
          else if(strncmp(optarg, "warn", MIN(strlen(optarg), (unsigned long)4))
                  == 0)
          {
            cSetLogLevel(eLogWarn);
          }
          else if(strncmp(optarg, "error",
                          MIN(strlen(optarg), (unsigned long)5))
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
