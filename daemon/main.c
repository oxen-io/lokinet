#include <llarp.h>
#include <stdio.h>

struct llarp_router * router;
struct llarp_config * gconfig;
struct llarp_ev_loop * mainloop;

int main(int argc, char * argv[])
{
  const char * conffname = "daemon.ini";
  if (argc > 1)
    conffname = argv[1];
  llarp_mem_jemalloc();
  llarp_new_config(&gconfig);
  llarp_ev_loop_alloc(&mainloop);
  printf("%s loaded\n", LLARP_VERSION);
  if(!llarp_load_config(gconfig, conffname))
  {
    printf("Loaded config %s\n", conffname);
    llarp_init_router(&router);
    if(!llarp_configure_router(router, gconfig))
    {
      printf("Running\n");
      llarp_run_router(router, mainloop);
      llarp_ev_loop_run(mainloop);
    }
    else
      printf("Failed to configure router\n");
  }
  else
    printf("Failed to load config %s\n", conffname);
  
  printf("Shutting down.");
  llarp_free_router(&router);
  printf(".");
  llarp_free_config(&gconfig);
  printf(".");
  llarp_ev_loop_free(&mainloop);
  printf(".\n");
  return 0;
}
