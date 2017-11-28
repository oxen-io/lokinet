#ifndef SARP_EV_H
#define SARP_EV_H
#include <sarp/mem.h>

#ifdef __cplusplus
extern "C" {
#endif

  struct sarp_ev_loop;

  void sarp_ev_loop_alloc(struct sarp_ev_loop ** ev, struct sarp_alloc * mem);
  void sarp_ev_loop_free(struct sarp_ev_loop ** ev);

  int sarp_ev_loop_run(struct sarp_ev_loop * ev);
  
#ifdef __cplusplus
}
#endif
#endif
