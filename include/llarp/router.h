#ifndef LLARP_ROUTER_H_
#define LLARP_ROUTER_H_
#include <llarp/config.h>
#include <llarp/ev.h>
#include <llarp/logic.hpp>
#include <llarp/threadpool.h>
#include <llarp/buffer.h>

struct llarp_nodedb;
struct llarp_router;

bool
llarp_findOrCreateIdentity(struct llarp_crypto *crypto, const char *path,
                           byte_t *secretkey);

struct llarp_router *
llarp_init_router(struct llarp_threadpool *worker,
                  struct llarp_ev_loop *netloop, struct llarp_logic *logic);
void
llarp_free_router(struct llarp_router **router);
bool
llarp_configure_router(struct llarp_router *router, struct llarp_config *conf);

bool
llarp_run_router(struct llarp_router *router, struct llarp_nodedb *nodedb);

void
llarp_stop_router(struct llarp_router *router);

#endif
