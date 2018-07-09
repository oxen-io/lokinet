#ifndef LLARP_ROUTER_H_
#define LLARP_ROUTER_H_
#include <llarp/config.h>
#include <llarp/ev.h>
#include <llarp/logic.h>
#include <llarp/nodedb.h>
#include <llarp/pathbuilder.h>
#include <llarp/router_contact.h>
#include <llarp/threadpool.h>

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
llarp_router_try_connect(struct llarp_router *router, struct llarp_rc *remote,
                         uint16_t numretries);

/// override default path builder function (FFI)
void
llarp_router_override_path_selection(struct llarp_router *router,
                                     llarp_pathbuilder_select_hop_func func);

bool
llarp_configure_router(struct llarp_router *router, struct llarp_config *conf);

void
llarp_run_router(struct llarp_router *router, struct llarp_nodedb *nodedb);

void
llarp_stop_router(struct llarp_router *router);

struct llarp_router_link_iter
{
  void *user;
  bool (*visit)(struct llarp_router_link_iter *, struct llarp_router *,
                struct llarp_link *);
};

void
llarp_router_iterate_links(struct llarp_router *router,
                           struct llarp_router_link_iter iter);

#endif
