#ifndef LLARP_PATHFINDER_HPP_
#define LLARP_PATHFINDER_HPP_
#include <llarp/pathfinder.h>

struct llarp_pathfinder_context {
  struct llarp_router* router;
  struct llarp_dht_context* dht;
  /// copy cstr
  llarp_pathfinder_context(llarp_router *p_router, struct llarp_dht_context* p_dht);
};

#endif
