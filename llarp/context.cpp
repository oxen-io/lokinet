#include <llarp.h>
#include <signal.h>
#include <llarp.hpp>
#include "logger.hpp"
#include "router.hpp"

#if(__FreeBSD__)
#include <pthread_np.h>
#endif

namespace llarp
{
  Context::Context(std::ostream &stdout, bool singleThread)
      : singleThreaded(singleThread), out(stdout)
  {
    llarp::Info(LLARP_VERSION, " ", LLARP_RELEASE_MOTTO);
  }

  Context::~Context()
  {
  }

  void
  Context::progress()
  {
    out << "." << std::flush;
  }

  bool
  Context::ReloadConfig()
  {
    llarp::Info("loading config at ", configfile);
    if(!llarp_load_config(config, configfile.c_str()))
    {
      llarp_config_iterator iter;
      iter.user  = this;
      iter.visit = &iter_config;
      llarp_config_iter(config, &iter);
      llarp::Info("config loaded");
      return true;
    }
    llarp_free_config(&config);
    llarp::Error("failed to load config file ", configfile);
    return false;
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
      if(!strcmp(key, "net-threads"))
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
  Context::Run()
  {
    llarp::Info("starting up");
    llarp_ev_loop_alloc(&mainloop);
    llarp_crypto_libsodium_init(&crypto);
    nodedb = llarp_nodedb_new(&crypto);
    if(!nodedb_dir[0])
    {
      llarp::Error("no nodedb_dir configured");
      return 0;
    }

    nodedb_dir[sizeof(nodedb_dir) - 1] = 0;
    if(!llarp_nodedb_ensure_dir(nodedb_dir))
    {
      llarp::Error("nodedb_dir is incorrect");
      return 0;
    }
    llarp::Info("nodedb_dir configured!");
    ssize_t loaded = llarp_nodedb_load_dir(nodedb, nodedb_dir);
    llarp::Info("nodedb_dir loaded ", loaded, " RCs");
    if(loaded < 0)
    {
      // shouldn't be possible
      llarp::Error("nodedb_dir directory doesn't exist");
      return 0;
    }

    // ensure worker thread pool
    if(!worker && !singleThreaded)
      worker = llarp_init_threadpool(2, "llarp-worker");
    else if(singleThreaded)
    {
      llarp::Info("running in single threaded mode");
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

    if(llarp_configure_router(router, config))
    {
      if(custom_dht_func)
      {
        llarp::Info("using custom dht function");
        llarp_dht_set_msg_handler(router->dht, custom_dht_func);
      }
      llarp_run_router(router, nodedb);
      // run net io thread
      if(singleThreaded)
      {
        llarp::Info("running mainloop");
        llarp_ev_loop_run_single_process(mainloop, worker, logic);
      }
      else
      {
        auto netio = mainloop;
        while(num_nethreads--)
        {
          netio_threads.emplace_back([netio]() { llarp_ev_loop_run(netio); });
#if(__APPLE__ && __MACH__)

#elif(__FreeBSD__)
          pthread_set_name_np(netio_threads.back().native_handle(),
                              "llarp-netio");
#else
          pthread_setname_np(netio_threads.back().native_handle(),
                             "llarp-netio");
#endif
        }
        llarp::Info("running mainloop");
        llarp_logic_mainloop(logic);
      }
      return 0;
    }
    else
      llarp::Error("Failed to configure router");
    return 1;
  }

  void
  Context::HandleSignal(int sig)
  {
    if(sig == SIGINT)
    {
      llarp::Info("SIGINT");
      SigINT();
    }
    if(sig == SIGHUP)
    {
      llarp::Info("SIGHUP");
      ReloadConfig();
    }
  }

  void
  Context::SigINT()
  {
    Close();
  }

  void
  Context::Close()
  {
    llarp::Debug("stop router");
    if(router)
      llarp_stop_router(router);

    llarp::Debug("stop workers");
    if(worker)
      llarp_threadpool_stop(worker);

    llarp::Debug("join workers");
    if(worker)
      llarp_threadpool_join(worker);

    llarp::Debug("stop logic");

    if(logic)
      llarp_logic_stop(logic);

    llarp::Debug("free config");
    llarp_free_config(&config);

    llarp::Debug("free workers");
    llarp_free_threadpool(&worker);

    llarp::Debug("free nodedb");
    llarp_nodedb_free(&nodedb);

    for(size_t i = 0; i < netio_threads.size(); ++i)
    {
      if(mainloop)
      {
        llarp::Debug("stopping event loop thread ", i);
        llarp_ev_loop_stop(mainloop);
      }
    }

    llarp::Debug("free router");
    llarp_free_router(&router);

    llarp::Debug("free logic");
    llarp_free_logic(&logic);

    for(auto &t : netio_threads)
    {
      llarp::Debug("join netio thread");
      t.join();
    }

    netio_threads.clear();
    llarp::Debug("free mainloop");
    llarp_ev_loop_free(&mainloop);
  }

  bool
  Context::LoadConfig(const std::string &fname)
  {
    llarp_new_config(&config);
    configfile = fname;
    return ReloadConfig();
  }
}

extern "C" {
struct llarp_main
{
  std::unique_ptr< llarp::Context > ctx;
};

struct llarp_main *
llarp_main_init(const char *fname, bool multiProcess)
{
  if(!fname)
    fname = "daemon.ini";

  llarp_main *m = new llarp_main;
  m->ctx.reset(new llarp::Context(std::cout, !multiProcess));
  if(!m->ctx->LoadConfig(fname))
  {
    m->ctx->Close();
    delete m;
    return nullptr;
  }
  return m;
}

void
llarp_main_set_dht_handler(struct llarp_main *ptr, llarp_dht_msg_handler func)
{
  ptr->ctx->custom_dht_func = func;
}

void
llarp_main_signal(struct llarp_main *ptr, int sig)
{
  ptr->ctx->HandleSignal(sig);
}

int
llarp_main_run(struct llarp_main *ptr)
{
  return ptr->ctx->Run();
}

void
llarp_main_free(struct llarp_main *ptr)
{
  delete ptr;
}
}
