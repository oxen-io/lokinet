#ifndef LLARP_ROUTER_H_
#define LLARP_ROUTER_H_
#include <llarp/config.h>
#include <llarp/ev.h>
#include <llarp/ibmq.h>
#include <llarp/obmd.h>
#include <llarp/threadpool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct llarp_router;

struct llarp_router *llarp_init_router(struct llarp_threadpool *worker);
void llarp_free_router(struct llarp_router **router);

bool llarp_configure_router(struct llarp_router *router,
                            struct llarp_config *conf);

void llarp_run_router(struct llarp_router *router, struct llarp_threadpool * logic);
void llarp_stop_router(struct llarp_router *router);

/** get router's inbound link level frame queue */
struct llarp_link_queue *llarp_router_link_queue(struct llarp_router *router);
/** get router's outbound link level frame dispatcher */
struct llarp_link_dispatcher *llarp_router_link_dispatcher(
    struct llarp_router *router);

#ifdef __cplusplus
}
#endif

#endif
