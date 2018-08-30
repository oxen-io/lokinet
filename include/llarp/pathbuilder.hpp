#ifndef LLARP_PATHBUILDER_HPP_
#define LLARP_PATHBUILDER_HPP_

#include <llarp/pathset.hpp>

namespace llarp
{
  namespace path
  {
    struct Builder : public PathSet
    {
      struct llarp_router* router;
      struct llarp_dht_context* dht;
      llarp::SecretKey enckey;
      size_t numHops;
      /// construct
      Builder(llarp_router* p_router, struct llarp_dht_context* p_dht,
              size_t numPaths, size_t numHops);

      virtual ~Builder();

      virtual bool
      SelectHop(llarp_nodedb* db, const RouterContact& prev, RouterContact& cur,
                size_t hop);

      virtual bool
      ShouldBuildMore() const;

      void
      BuildOne();

      void
      ManualRebuild(size_t N);

      virtual const byte_t*
      GetTunnelEncryptionSecretKey() const;
    };
  }  // namespace path

}  // namespace llarp
#endif
