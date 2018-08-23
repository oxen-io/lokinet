#ifndef LLARP_PATHBUILDER_HPP_
#define LLARP_PATHBUILDER_HPP_
#include <llarp/pathbuilder.h>
#include <llarp/router.h>
#include <llarp/path.hpp>
#include <llarp/pathset.hpp>

struct llarp_pathbuilder_context : public llarp::path::PathSet
{
  struct llarp_router* router;
  struct llarp_dht_context* dht;
  llarp::SecretKey enckey;
  size_t numHops;
  /// construct
  llarp_pathbuilder_context(llarp_router* p_router,
                            struct llarp_dht_context* p_dht, size_t numPaths,
                            size_t numHops);

  virtual ~llarp_pathbuilder_context();

  virtual bool
  SelectHop(llarp_nodedb* db, llarp_rc* prev, llarp_rc* cur, size_t hop);

  virtual bool
  ShouldBuildMore() const;

  void
  BuildOne();

  void
  ManualRebuild(size_t N);

  virtual byte_t*
  GetTunnelEncryptionSecretKey();
};

#endif
