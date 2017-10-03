#ifndef SARP_ROUTER_H_
#define SARP_ROUTER_H_
#include <sarp/config.h>
#include <sarp/mem.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sarp_router;

void sarp_init_router(struct sarp_router ** router, struct sarp_alloc * mem);
void sarp_free_router(struct sarp_router ** router);

int sarp_configure_router(struct sarp_router * router, struct sarp_config * conf);
  
void sarp_run_router(struct sarp_router * router);
void sarp_stop_router(struct sarp_router * router);

#ifdef __cplusplus
}
#endif

#endif
