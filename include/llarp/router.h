#ifndef LLARP_ROUTER_H_
#define LLARP_ROUTER_H_
#include <llarp/config.h>
#include <llarp/ev.h>

#ifdef __cplusplus
extern "C" {
#endif

  struct llarp_router;

  void llarp_init_router(struct llarp_router ** router);
  void llarp_free_router(struct llarp_router ** router);

  int llarp_configure_router(struct llarp_router * router, struct llarp_config * conf);
  
  void llarp_run_router(struct llarp_router * router, struct llarp_ev_loop * loop);
  void llarp_stop_router(struct llarp_router * router);

#ifdef __cplusplus
}
#endif

#endif
