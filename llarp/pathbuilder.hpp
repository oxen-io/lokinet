#ifndef LLARP_PATHFINDER_HPP_
#define LLARP_PATHFINDER_HPP_
#include <llarp/pathbuilder.h>

struct llarp_pathbuilder_context : public llarp::path::PathSet
{
  struct llarp_router* router;
  struct llarp_dht_context* dht;
  /// construct
  llarp_pathbuilder_context(llarp_router* p_router,
                            struct llarp_dht_context* p_dht);

  void
  BuildOne();
};

#endif
