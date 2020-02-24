#ifndef LLARP_PATHBUILDER_HPP
#define LLARP_PATHBUILDER_HPP

#include <path/pathset.hpp>
#include <util/status.hpp>

#include <atomic>
#include <set>

namespace llarp
{
  namespace path
  {
    // milliseconds waiting between builds on a path
    static constexpr auto MIN_PATH_BUILD_INTERVAL = 500ms;

    struct Builder : public PathSet
    {
     private:
      llarp_time_t m_LastWarn = 0s;

     protected:
      /// flag for PathSet::Stop()
      std::atomic< bool > _run;

      virtual bool
      UrgentBuild(llarp_time_t now) const;

     private:
      void
      DoPathBuildBackoff();

      bool
      DoUrgentBuildAlignedTo(const RouterID remote,
                             std::vector< RouterContact >& hops);

      bool
      DoBuildAlignedTo(const RouterID remote,
                       std::vector< RouterContact >& hops);

     public:
      AbstractRouter* m_router;
      SecretKey enckey;
      size_t numHops;
      llarp_time_t lastBuild          = 0s;
      llarp_time_t buildIntervalLimit = MIN_PATH_BUILD_INTERVAL;

      /// construct
      Builder(AbstractRouter* p_router, size_t numPaths, size_t numHops);

      virtual ~Builder() = default;

      util::StatusObject
      ExtractStatus() const;

      bool
      SelectHop(llarp_nodedb* db, const std::set< RouterID >& prev,
                RouterContact& cur, size_t hop, PathRole roles) override;

      bool
      ShouldBuildMore(llarp_time_t now) const override;

      /// should we bundle RCs in builds?
      virtual bool
      ShouldBundleRC() const = 0;

      void
      ResetInternalState() override;

      /// return true if we hit our soft limit for building paths too fast
      bool
      BuildCooldownHit(llarp_time_t now) const;

      /// get roles for this path builder
      virtual PathRole
      GetRoles() const
      {
        return ePathRoleAny;
      }

      bool
      Stop() override;

      bool
      IsStopped() const override;

      bool
      ShouldRemove() const override;

      llarp_time_t
      Now() const override;

      void
      Tick(llarp_time_t now) override;

      void
      BuildOne(PathRole roles = ePathRoleAny) override;

      bool
      BuildOneAlignedTo(const RouterID endpoint) override;

      void
      Build(const std::vector< RouterContact >& hops,
            PathRole roles = ePathRoleAny) override;

      bool
      SelectHops(llarp_nodedb* db, std::vector< RouterContact >& hops,
                 PathRole roles = ePathRoleAny);

      void
      ManualRebuild(size_t N, PathRole roles = ePathRoleAny);

      virtual const SecretKey&
      GetTunnelEncryptionSecretKey() const;

      virtual void
      HandlePathBuilt(Path_ptr p) override;

      virtual void
      HandlePathBuildTimeout(Path_ptr p) override;

      virtual void
      HandlePathBuildFailed(Path_ptr p) override;
    };

    using Builder_ptr = std::shared_ptr< Builder >;

  }  // namespace path

}  // namespace llarp
#endif
