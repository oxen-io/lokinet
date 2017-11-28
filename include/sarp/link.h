#ifndef SARP_LINK_H_
#define SARP_LINK_H_
#include <sarp/config.h>
#include <sarp/mem.h>
#include <sarp/router.h>
#include <sarp/ev.h>

#ifdef __cplusplus
extern "C" {
#endif

  struct sarp_inet_link;

  void sarp_inet_link_alloc(struct sarp_inet_link ** link, struct sarp_alloc * mem);
  void sarp_inet_link_free(struct sarp_inet_link ** link);

  void sarp_inet_link_configure(struct sarp_inet_link * link, struct sarp_config * conf);

  void sarp_inet_link_start(struct sarp_inet_link * link, struct sarp_router * router);

  void sarp_inet_link_stop(struct sarp_inet_link * link);
  
#ifdef __cplusplus
}
#endif

#endif
