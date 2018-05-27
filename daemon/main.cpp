#include <llarp.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <llarp/logger.hpp>

#include <thread>
#include <vector>

static void
progress()
{
  printf(".");
  fflush(stdout);
}

struct llarp_main
{
  struct llarp_alloc mem;
  int num_nethreads = 1;
  std::vector< std::thread > netio_threads;
  struct llarp_crypto crypto;
  struct llarp_router *router     = nullptr;
  struct llarp_threadpool *worker = nullptr;
  struct llarp_logic *logic       = nullptr;
  struct llarp_config *config     = nullptr;
  struct llarp_nodedb *nodedb     = nullptr;
  struct llarp_ev_loop *mainloop  = nullptr;
  char nodedb_dir[256];
  int exitcode;

  int
  shutdown()
  {
    printf("Shutting down ");

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

    printf("stopped\n");
    fflush(stdout);
    return exitcode;
  }
};

void
iter_main_config(struct llarp_config_iterator *itr, const char *section,
                 const char *key, const char *val)
{
  llarp_main *m = static_cast< llarp_main * >(itr->user);

  if(!strcmp(section, "router"))
  {
    if(!strcmp(key, "worker-threads"))
    {
      int workers = atoi(val);
      if(workers > 0 && m->worker == nullptr)
      {
        m->worker = llarp_init_threadpool(workers, "llarp-worker");
      }
    }
    if(!strcmp(key, "net-threads"))
    {
      m->num_nethreads = atoi(val);
      if(m->num_nethreads <= 0)
        m->num_nethreads = 1;
    }
  }
  if(!strcmp(section, "netdb"))
  {
    if(!strcmp(key, "dir"))
    {
      strncpy(m->nodedb_dir, val, sizeof(m->nodedb_dir));
    }
  }
}

llarp_main *sllarp = nullptr;

void
handle_signal(int sig)
{
  if(sllarp->logic)
    llarp_logic_stop(sllarp->logic);
  if(sllarp->mainloop)
    llarp_ev_loop_stop(sllarp->mainloop);

  if(sllarp)
  {
    for(auto &t : sllarp->netio_threads)
    {
      progress();
      t.join();
    }
    progress();
    sllarp->netio_threads.clear();
  }
  printf("\ninterrupted\n");
}

int
main(int argc, char *argv[])
{
  const char *conffname = "daemon.ini";
  if(argc > 1)
    conffname = argv[1];
  sllarp = new llarp_main;
  llarp_mem_stdlib(&sllarp->mem);
  auto mem = &sllarp->mem;
  llarp_new_config(&sllarp->config);
  llarp_ev_loop_alloc(&sllarp->mainloop);
  llarp_crypto_libsodium_init(&sllarp->crypto);
  llarp::Info(__FILE__, LLARP_VERSION, " loading config at ", conffname);
  if(!llarp_load_config(sllarp->config, conffname))
  {
    llarp_config_iterator iter;
    iter.user  = sllarp;
    iter.visit = iter_main_config;
    llarp_config_iter(sllarp->config, &iter);

    sllarp->nodedb = llarp_nodedb_new(mem, &sllarp->crypto);

    if(sllarp->nodedb_dir[0])
    {
      sllarp->nodedb_dir[sizeof(sllarp->nodedb_dir) - 1] = 0;
      if(llarp_nodedb_ensure_dir(sllarp->nodedb_dir))
      {
        // ensure worker thread pool
        if(!sllarp->worker)
          sllarp->worker = llarp_init_threadpool(2, "llarp-worker");
        // ensure netio thread
        sllarp->logic = llarp_init_logic(mem);

        sllarp->router = llarp_init_router(mem, sllarp->worker,
                                           sllarp->mainloop, sllarp->logic);

        if(llarp_configure_router(sllarp->router, sllarp->config))
        {
          signal(SIGINT, handle_signal);

          llarp_run_router(sllarp->router);
          // run net io thread
          auto netio = sllarp->mainloop;
          while(sllarp->num_nethreads--)
          {
            sllarp->netio_threads.emplace_back(
                [netio]() { llarp_ev_loop_run(netio); });
            pthread_setname_np(sllarp->netio_threads.back().native_handle(),
                               "llarp-netio");
          }
          llarp::Info(__FILE__, "running");
          sllarp->exitcode = 0;
          llarp_logic_mainloop(sllarp->logic);
        }
        else
          llarp::Error(__FILE__, "failed to start router");
      }
      else
        llarp::Error(__FILE__, "Failed to initialize nodedb");
    }
    else
      llarp::Error(__FILE__, "no nodedb defined");
    auto code = sllarp->shutdown();
    delete sllarp;
    sllarp = nullptr;
    return code;
  }
  else
    llarp::Error(__FILE__, "failed to load config");
  delete sllarp;
  return 1;
}
