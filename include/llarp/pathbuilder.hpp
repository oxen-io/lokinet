#ifndef LLARP_PATHBUILDER_HPP_
#define LLARP_PATHBUILDER_HPP_
#include <llarp/pathbuilder.h>
#include <llarp/router.h>
#include <llarp/pathset.hpp>

struct llarp_pathbuilder_context : public llarp::path::PathSet
{
  struct llarp_router* router;
  struct llarp_dht_context* dht;
  /// construct
  llarp_pathbuilder_context(llarp_router* p_router,
                            struct llarp_dht_context* p_dht);

  virtual ~llarp_pathbuilder_context(){};

  void
  BuildOne();
};

#endif
