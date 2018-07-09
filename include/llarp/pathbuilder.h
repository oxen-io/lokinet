#ifndef LLARP_PATHFINDER_H_
#define LLARP_PATHFINDER_H_

#include <llarp/buffer.h>
#include <llarp/path.h>

/**
 * path_base.h
 *
 * path api functions
 */

/// forard declare
struct llarp_router;
struct llarp_dht_context;

// fwd declr
struct llarp_pathbuilder_context;

/// alloc
struct llarp_pathbuilder_context*
llarp_pathbuilder_context_new(struct llarp_router* router,
                              struct llarp_dht_context* dht);
/// dealloc
void
llarp_pathbuilder_context_free(struct llarp_pathbuilder_context* ctx);

// fwd declr
struct llarp_pathbuild_job;

/// response callback
typedef void (*llarp_pathbuilder_hook)(struct llarp_pathbuild_job*);
// select hop function (nodedb, prevhop, result, hopnnumber) called in logic
// thread
typedef void (*llarp_pathbuilder_select_hop_func)(struct llarp_nodedb*,
                                                  struct llarp_rc*,
                                                  struct llarp_rc*, size_t);

// request struct
struct llarp_pathbuild_job
{
  // opaque pointer for user data
  void* user;
  // router context (set by llarp_pathbuilder_build_path)
  struct llarp_router* router;
  // context
  struct llarp_pathbuilder_context* context;
  // path hop selection
  llarp_pathbuilder_select_hop_func selectHop;
  // called when the path build started
  llarp_pathbuilder_hook pathBuildStarted;
  // path
  struct llarp_path_hops hops;
};

/// request func
// or find_path but thought pathfinder_find_path was a bit redundant
void
llarp_pathbuilder_build_path(struct llarp_pathbuild_job* job);

#endif
