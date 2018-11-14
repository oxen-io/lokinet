#ifndef LLARP_PATHBUILDER_HPP_
#define LLARP_PATHBUILDER_HPP_

#include <llarp/pathset.hpp>

namespace llarp
{
  namespace path
  {
    // milliseconds waiting between builds on a path
    constexpr llarp_time_t MIN_PATH_BUILD_INTERVAL = 10 * 1000;

    struct Builder : public PathSet
    {
      struct llarp_router* router;
      struct llarp_dht_context* dht;
      llarp::SecretKey enckey;
      size_t numHops;
      llarp_time_t lastBuild          = 0;
      llarp_time_t buildIntervalLimit = MIN_PATH_BUILD_INTERVAL;
      /// construct
      Builder(llarp_router* p_router, struct llarp_dht_context* p_dht,
              size_t numPaths, size_t numHops);

      virtual ~Builder();

      virtual bool
      SelectHop(llarp_nodedb* db, const RouterContact& prev, RouterContact& cur,
                size_t hop, PathRole roles);

      virtual bool
      ShouldBuildMore(llarp_time_t now) const;

      llarp_time_t
      Now() const;

      void
      BuildOne(PathRole roles = ePathRoleAny);

      void
      Build(const std::vector< RouterContact >& hops,
            PathRole roles = ePathRoleAny);

      bool
      SelectHops(llarp_nodedb* db, std::vector< RouterContact >& hops,
                 PathRole roles = ePathRoleAny);

      void
      ManualRebuild(size_t N, PathRole roles = ePathRoleAny);

      virtual const byte_t*
      GetTunnelEncryptionSecretKey() const;

      virtual void
      HandlePathBuilt(Path* p);

      virtual void
      HandlePathBuildTimeout(Path* p);
    };
  }  // namespace path

}  // namespace llarp
#endif
