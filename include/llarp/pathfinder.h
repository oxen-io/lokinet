#ifndef LLARP_PATHFINDER_H_
#define LLARP_PATHFINDER_H_

#include <llarp/buffer.h>
#include <llarp/router.h>
#include <vector>

/**
 * path_base.h
 *
 * path api functions
 */

#ifdef __cplusplus
extern "C" {
#endif

// fwd declr
struct llarp_pathfinder_context;

/// alloc
struct llarp_pathfinder_context *
  llarp_pathfinder_context_new(struct llarp_router* router,
                               struct llarp_dht_context* dht);
/// dealloc
void
  llarp_pathfinder_context_free(struct llarp_pathfinder_context* ctx);

// fwd declr
struct llarp_get_route;

/// response callback
typedef void (*llarp_pathfinder_response)(struct llarp_get_route *);

// request struct
struct llarp_get_route
{
  // context
  struct llarp_pathfinder_context* pathfinder;
  // parameter
  byte_t destination[PUBKEYSIZE];
  // result
    std::vector<llarp_rc> route;
};
/// request func
// or find_path but thought pathfinder_find_path was a bit redundant
void llarp_pathfinder_get_route(struct llarp_pathfinder_context* pathfinder);

#ifdef __cplusplus
}
#endif
#endif
