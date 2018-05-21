#ifndef LLARP_ROUTER_H_
#define LLARP_ROUTER_H_
#include <llarp/config.h>
#include <llarp/ev.h>
#include <llarp/ibmq.h>
#include <llarp/logic.h>
#include <llarp/obmd.h>
#include <llarp/threadpool.h>

#ifdef __cplusplus
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

extern "C" {
#endif

struct llarp_router;

bool llarp_findOrCreateIdentity(llarp_crypto *crypto, fs::path path, llarp_seckey_t *identity);
bool llarp_rc_write(struct llarp_rc *rc, fs::path our_rc_file);

struct llarp_router *llarp_init_router(struct llarp_alloc * mem, struct llarp_threadpool *worker, struct llarp_ev_loop * netloop, struct llarp_logic *logic);
void llarp_free_router(struct llarp_router **router);

bool llarp_configure_router(struct llarp_router *router,
                            struct llarp_config *conf);

void llarp_rc_clear(struct llarp_rc *rc);
bool llarp_rc_addr_list_iter(struct llarp_ai_list_iter *iter,
                                    struct llarp_ai *ai);
void llarp_rc_set_addrs(struct llarp_rc *rc, struct llarp_alloc *mem, struct llarp_ai_list *addr);
void llarp_rc_set_pubkey(struct llarp_rc *rc, uint8_t *pubkey);
void llarp_rc_sign(llarp_crypto *crypto, llarp_seckey_t *identity,
                   struct llarp_rc *rc);
void llarp_run_router(struct llarp_router *router);
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
