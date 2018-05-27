#include <llarp.h>
#include <signal.h>
#include <llarp.hpp>
#include "logger.hpp"

namespace llarp
{
  Context::Context(std::ostream &stdout) : out(stdout)
  {
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
    llarp::Info(__FILE__, LLARP_VERSION, " loading config at ", configfile);
    if(!llarp_load_config(config, configfile.c_str()))
    {
      llarp_config_iterator iter;
      iter.user  = this;
      iter.visit = &iter_config;
      llarp_config_iter(config, &iter);
      return true;
    }
    llarp_free_config(&config);
    llarp::Error(__FILE__, "failed to load config file ", configfile);
    return false;
  }

  void
  Context::iter_config(llarp_config_iterator *itr, const char *section,
                       const char *key, const char *val)
  {
    Context *ctx = static_cast< Context * >(itr->user);
    if(!strcmp(section, "router"))
    {
      if(!strcmp(key, "worker-threads"))
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
    llarp_mem_stdlib(&mem);
    llarp_ev_loop_alloc(&mainloop);
    llarp_crypto_libsodium_init(&crypto);
    nodedb = llarp_nodedb_new(&mem, &crypto);
    if(nodedb_dir[0])
    {
      nodedb_dir[sizeof(nodedb_dir) - 1] = 0;
      if(llarp_nodedb_ensure_dir(nodedb_dir))
      {
        // ensure worker thread pool
        if(!worker)
          worker = llarp_init_threadpool(2, "llarp-worker");
        // ensure netio thread
        logic = llarp_init_logic(&mem);

        router = llarp_init_router(&mem, worker, mainloop, logic);

        if(llarp_configure_router(router, config))
        {
          llarp_run_router(router);
          // run net io thread
          auto netio = mainloop;
          while(num_nethreads--)
          {
            netio_threads.emplace_back([netio]() { llarp_ev_loop_run(netio); });
            pthread_setname_np(netio_threads.back().native_handle(),
                               "llarp-netio");
          }
          llarp::Info(__FILE__, "running");
          llarp_logic_mainloop(logic);
          return 0;
        }
        else
          llarp::Error(__FILE__, "failed to start router");
      }
      else
        llarp::Error(__FILE__, "Failed to initialize nodedb");
    }
    else
      llarp::Error(__FILE__, "no nodedb defined");
    return 1;
  }

  void
  Context::HandleSignal(int sig)
  {
    if(sig == SIGINT)
    {
      llarp::Info(__FILE__, "SIGINT");
      SigINT();
    }
    if(sig == SIGHUP)
    {
      llarp::Info(__FILE__, "SIGHUP");
      ReloadConfig();
    }
  }

  void
  Context::SigINT()
  {
    if(logic)
      llarp_logic_stop(logic);
    if(mainloop)
      llarp_ev_loop_stop(mainloop);
    for(auto &t : netio_threads)
    {
      t.join();
    }
    netio_threads.clear();
  }

  void
  Context::Close()
  {
    progress();
    if(mainloop)
      llarp_ev_loop_stop(mainloop);

    progress();
    if(worker)
      llarp_threadpool_stop(worker);

    progress();

    if(worker)
      llarp_threadpool_join(worker);

    progress();
    if(logic)
      llarp_logic_stop(logic);

    progress();

    if(router)
      llarp_stop_router(router);

    progress();
    llarp_free_router(&router);

    progress();
    llarp_free_config(&config);

    progress();
    llarp_ev_loop_free(&mainloop);

    progress();
    llarp_free_threadpool(&worker);

    progress();

    llarp_free_logic(&logic);

    progress();
    llarp_nodedb_free(&nodedb);

    for(auto &t : netio_threads)
    {
      progress();
      t.join();
    }
    progress();
    netio_threads.clear();
    out << std::endl << "done" << std::endl;
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

bool
llarp_main_init(struct llarp_main **ptr, const char *fname)
{
  if(!fname)
    return false;

  llarp_main *m = new llarp_main;
  m->ctx.reset(new llarp::Context(std::cout));
  if(!m->ctx->LoadConfig(fname))
  {
    m->ctx->Close();
    delete m;
    return false;
  }
  *ptr = m;
  return true;
}

void
llarp_main_signal(struct llarp_main *ptr, int sig)
{
  ptr->ctx->HandleSignal(sig);
}

int
llarp_main_run(struct llarp_main *ptr)
{
  auto code = ptr->ctx->Run();
  ptr->ctx->Close();
  return code;
}

void
llarp_main_free(struct llarp_main **ptr)
{
  if(*ptr)
    delete *ptr;
  *ptr = nullptr;
}
}
