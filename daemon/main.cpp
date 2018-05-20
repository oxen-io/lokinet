#include <llarp.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>


static void progress() {
  printf(".");
  fflush(stdout);
}

struct llarp_main
{
  struct llarp_alloc mem;
  struct llarp_crypto crypto;
  struct llarp_router *router = nullptr;
  struct llarp_threadpool *worker = nullptr;
  struct llarp_threadpool *thread = nullptr;
  struct llarp_logic *logic = nullptr;
  struct llarp_config *config = nullptr; 
  struct llarp_nodedb *nodedb = nullptr;
  struct llarp_ev_loop *mainloop = nullptr;
  char nodedb_dir[256];
  int exitcode;

  int shutdown()
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
    if (logic)
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
  
    printf("\n");
    fflush(stdout);
    return exitcode;
  }
  
};

void iter_main_config(struct llarp_config_iterator *itr, const char *section,
                      const char *key, const char *val) {
  llarp_main *m = static_cast<llarp_main *>(itr->user);

  if (!strcmp(section, "router")) {
    if (!strcmp(key, "threads")) {
      int workers = atoi(val);
      if (workers > 0 && m->worker == NULL) {
        m->worker = llarp_init_threadpool(workers);
      }
    }
  }
  if (!strcmp(section, "netdb")) {
    if (!strcmp(key, "dir")) {
      strncpy(m->nodedb_dir, val, sizeof(m->nodedb_dir));
    }
  }
}


llarp_main llarp;

void run_net(void * user)
{
  llarp_ev_loop_run(static_cast<llarp_ev_loop*>(user));
}

void handle_signal(int sig)
{
  printf("interrupted\n");
  llarp_ev_loop_stop(llarp.mainloop);
  llarp_logic_stop(llarp.logic);
  printf("closing...");
}

int main(int argc, char *argv[]) {
  const char *conffname = "daemon.ini";
  if (argc > 1) conffname = argv[1];
  llarp_mem_jemalloc(&llarp.mem);
  auto mem = &llarp.mem;
  llarp_new_config(&llarp.config);
  llarp_ev_loop_alloc(&llarp.mainloop);
  llarp_crypto_libsodium_init(&llarp.crypto);
  printf("%s loading config file %s\n", LLARP_VERSION, conffname);
  if (!llarp_load_config(llarp.config, conffname)) {
    llarp_config_iterator iter;
    iter.user = &llarp;
    iter.visit = iter_main_config;
    llarp_config_iter(llarp.config, &iter);

    llarp.nodedb = llarp_nodedb_new(mem, &llarp.crypto);
    
    if (llarp.nodedb_dir[0]) {
      llarp.nodedb_dir[sizeof(llarp.nodedb_dir) - 1] = 0;
      char *dir = llarp.nodedb_dir;
      if (llarp_nodedb_ensure_dir(dir)) {
        // ensure worker thread pool
        if (!llarp.worker) llarp.worker = llarp_init_threadpool(2);
        // ensure logic thread
        llarp.thread = llarp_init_threadpool(1);
        llarp.logic = llarp_init_logic(mem);
        
        llarp.router = llarp_init_router(mem, llarp.worker, llarp.mainloop, llarp.logic);

        if (llarp_configure_router(llarp.router, llarp.config)) {
          signal(SIGINT, handle_signal);
          printf("starting router\n");
          llarp_run_router(llarp.router);
          // run mainloop
          llarp_threadpool_queue_job(llarp.thread, {llarp.mainloop, &run_net});
          printf("running\n");
          llarp.exitcode = 0;
          llarp_logic_mainloop(llarp.logic);
        } else
          printf("Failed to configure router\n");
      } else
        printf("failed to initialize nodedb at %s\n", dir);
    } else
      printf("no nodedb defined\n");
    return llarp.shutdown();
  } else
    printf("Failed to load config %s\n", conffname);

  return 1;
}
