#include <llarp.h>
#include <stdio.h>
#include <string.h>

struct llarp_main {
  struct llarp_router *router;
  struct llarp_threadpool *tp;
  struct llarp_config *config;
  struct llarp_ev_loop *mainloop;
};

void iter_main_config(struct llarp_config_iterator *itr, const char *section,
                      const char *key, const char *val) {
  struct llarp_main *m = (struct llarp_main *)itr->user;
  if (!strcmp(section, "router")) {
    if (!strcmp(key, "threads")) {
      int workers = atoi(val);
      if (workers > 0 && m->tp == NULL) {
        printf("%s: %d worker threads\n", section, workers);
        m->tp = llarp_init_threadpool(workers);
      }
    }
  }
}

int shutdown_llarp(struct llarp_main *m) {
  printf("Shutting down .");
  llarp_stop_router(m->router);
  printf(".");
  fflush(stdout);
  llarp_ev_loop_stop(m->mainloop);
  printf(".");
  fflush(stdout);
  llarp_threadpool_join(m->tp);
  printf(".");
  fflush(stdout);
  llarp_free_router(&m->router);
  printf(".");
  fflush(stdout);
  llarp_free_config(&m->config);
  printf(".");
  fflush(stdout);
  llarp_ev_loop_free(&m->mainloop);
  printf(".");
  fflush(stdout);
  llarp_free_threadpool(&m->tp);
  printf(".\n");
  return 0;
}

int main(int argc, char *argv[]) {
  struct llarp_main llarp = {NULL, NULL, NULL, NULL};
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

    if (!llarp.tp) llarp.tp = llarp_init_threadpool(2);
    llarp.router = llarp_init_router(llarp.tp);

    if (llarp_configure_router(llarp.router, llarp.config)) {
      printf("Running\n");
      llarp_run_router(llarp.router, llarp.mainloop);
      llarp_ev_loop_run(llarp.mainloop);
    } else
      printf("Failed to configure router\n");
  } else
    printf("Failed to load config %s\n", conffname);

  return shutdown_llarp(&llarp);
}
