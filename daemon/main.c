#include <sarp.h>
#include <sarp/mem.h>
#include <sarp/ev.h>
#include <stdio.h>

struct sarp_router * router;
struct sarp_config * gconfig;
struct sarp_ev_loop * mainloop;

int main(int argc, char * argv[])
{
  const char * conffname = "daemon.ini";
  if (argc > 1)
    conffname = argv[1];
  sarp_mem_jemalloc();
  sarp_new_config(&gconfig);
  sarp_ev_loop_alloc(&mainloop);
  
  if(sarp_load_config(gconfig, conffname))
  {
    printf("!!! failed to load %s\n", conffname);
  }
  else
  {
    printf("loaded config %s\n", conffname);
    sarp_init_router(&router);
    sarp_configure_router(router, gconfig);
    printf("running...\n");
    sarp_run_router(router, mainloop);
    sarp_ev_loop_run(mainloop);
  }
  printf("shutting down.");
  sarp_free_router(&router);
  printf(".");
  sarp_free_config(&gconfig);
  printf(".");
  sarp_ev_loop_free(&mainloop);
  printf(".\n");
  return 0;
}
