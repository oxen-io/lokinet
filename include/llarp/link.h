#ifndef LLARP_LINK_H_
#define LLARP_LINK_H_
#include <llarp/config.h>
#include <llarp/mem.h>
#include <llarp/router.h>
#include <llarp/ev.h>

#ifdef __cplusplus
extern "C" {
#endif

  struct llarp_inet_link;

  void llarp_inet_link_alloc(struct llarp_inet_link ** link, struct llarp_alloc * mem);
  void llarp_inet_link_free(struct llarp_inet_link ** link);

  void llarp_inet_link_configure(struct llarp_inet_link * link, struct llarp_config * conf);

  void llarp_inet_link_start(struct llarp_inet_link * link, struct llarp_router * router);

  void llarp_inet_link_stop(struct llarp_inet_link * link);
  
#ifdef __cplusplus
}
#endif

#endif
