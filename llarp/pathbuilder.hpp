#ifndef LLARP_PATHBUILDER_HPP_
#define LLARP_PATHBUILDER_HPP_

#include <pathset.hpp>
#include <atomic>

namespace llarp
{
  namespace path
  {
    // milliseconds waiting between builds on a path
    constexpr llarp_time_t MIN_PATH_BUILD_INTERVAL = 10 * 1000;

    struct Builder : public PathSet
    {
     protected:
      /// flag for PathSet::Stop()
      std::atomic< bool > _run;

     public:
      bool
      CanBuildPaths() const
      {
        return _run.load();
      }

      llarp::Router* router;
      struct llarp_dht_context* dht;
      llarp::SecretKey enckey;
      size_t numHops;
      llarp_time_t lastBuild          = 0;
      llarp_time_t buildIntervalLimit = MIN_PATH_BUILD_INTERVAL;

      // how many keygens are currently happening
      std::atomic< uint8_t > keygens;

      /// construct
      Builder(llarp::Router* p_router, struct llarp_dht_context* p_dht,
              size_t numPaths, size_t numHops);

      virtual ~Builder();

      virtual bool
      SelectHop(llarp_nodedb* db, const RouterContact& prev, RouterContact& cur,
                size_t hop, PathRole roles) override;

      virtual bool
      ShouldBuildMore(llarp_time_t now) const override;

      /// return true if we hit our soft limit for building paths too fast
      bool
      BuildCooldownHit(llarp_time_t now) const;

      virtual bool
      Stop() override;

      bool
      ShouldRemove() const override;

      llarp_time_t
      Now() const override;

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

      virtual const SecretKey&
      GetTunnelEncryptionSecretKey() const;

      virtual void
      HandlePathBuilt(Path* p) override;

      virtual void
      HandlePathBuildTimeout(Path* p) override;
    };
  }  // namespace path

}  // namespace llarp
#endif
