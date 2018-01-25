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
  printf("%s loaded\n", SARP_VERSION);
  if(!sarp_load_config(gconfig, conffname))
  {
    printf("Loaded config %s\n", conffname);
    sarp_init_router(&router);
    if(!sarp_configure_router(router, gconfig))
    {
      printf("Running\n");
      sarp_run_router(router, mainloop);
      sarp_ev_loop_run(mainloop);
    }
    else
      printf("Failed to configure router\n");
  }
  else
    printf("Failed to load config %s\n", conffname);
  
  printf("Shutting down.");
  sarp_free_router(&router);
  printf(".");
  sarp_free_config(&gconfig);
  printf(".");
  sarp_ev_loop_free(&mainloop);
  printf(".\n");
  return 0;
}
