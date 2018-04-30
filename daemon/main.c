#include <llarp.h>
#include <stdio.h>
#include <string.h>

struct llarp_main {
  struct llarp_router *router;
  struct llarp_threadpool *worker;
  struct llarp_threadpool *logic;
  struct llarp_config *config;
  struct llarp_ev_loop *mainloop;
};

void iter_main_config(struct llarp_config_iterator *itr, const char *section,
                      const char *key, const char *val) {
  struct llarp_main *m = (struct llarp_main *)itr->user;
  if (!strcmp(section, "router")) {
    if (!strcmp(key, "threads")) {
      int workers = atoi(val);
      if (workers > 0 && m->worker == NULL) {
        printf("%s: %d worker threads\n", section, workers);
        m->worker = llarp_init_threadpool(workers);
      }
    }
  }
}

static void progress() {
  printf(".");
  fflush(stdout);
}

int shutdown_llarp(struct llarp_main *m) {
  printf("Shutting down ");
  progress();
  llarp_stop_router(m->router);
  progress();
  llarp_ev_loop_stop(m->mainloop);
  progress();
  llarp_threadpool_stop(m->worker);
  progress();
  llarp_threadpool_join(m->worker);
  progress();
  if (m->logic) llarp_threadpool_wait(m->logic);
  progress();
  if (m->logic) llarp_threadpool_join(m->logic);
  progress();
  llarp_free_router(&m->router);
  progress();
  llarp_free_config(&m->config);
  progress();
  llarp_ev_loop_free(&m->mainloop);
  progress();
  llarp_free_threadpool(&m->worker);
  progress();
  if (m->logic) llarp_free_threadpool(&m->logic);
  progress();
  printf("\n");
  fflush(stdout);
  return 0;
}

struct llarp_main llarp;

int main(int argc, char *argv[]) {
  memset(&llarp, 0, sizeof(struct llarp_main));
  const char *conffname = "daemon.ini";
  if (argc > 1) conffname = argv[1];
  llarp_mem_stdlib();
  llarp_new_config(&llarp.config);
  llarp_ev_loop_alloc(&llarp.mainloop);
  printf("%s loaded\n", LLARP_VERSION);
  if (!llarp_load_config(llarp.config, conffname)) {
    printf("Loaded config %s\n", conffname);
    struct llarp_config_iterator iter;
    iter.user = &llarp;
    iter.visit = iter_main_config;
    llarp_config_iter(llarp.config, &iter);

    if (!llarp.worker) llarp.worker = llarp_init_threadpool(2);
    llarp.router = llarp_init_router(llarp.worker);

    if (llarp_configure_router(llarp.router, llarp.config)) {
      llarp.logic = llarp_init_threadpool(1);
      printf("starting router\n");
      llarp_run_router(llarp.router, llarp.logic);
      printf("running mainloop\n");
      llarp_ev_loop_run(llarp.mainloop);
    } else
      printf("Failed to configure router\n");
  } else
    printf("Failed to load config %s\n", conffname);

  return shutdown_llarp(&llarp);
}
