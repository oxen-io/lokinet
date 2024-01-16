#pragma once

#include "pathset.hpp"

#include <llarp/util/decaying_hashset.hpp>
#include <llarp/util/status.hpp>

#include <atomic>
#include <set>

namespace llarp::path
{
  /// limiter for path builds
  /// prevents overload and such
  class BuildLimiter
  {
    util::DecayingHashSet<RouterID> m_EdgeLimiter;

   public:
    /// attempt a build
    /// return true if we are allowed to continue
    bool
    Attempt(const RouterID& router);

    /// decay limit entries
    void
    Decay(llarp_time_t now);

    /// return true if this router is currently limited
    bool
    Limited(const RouterID& router) const;
  };

  struct Builder : public PathSet
  {
   private:
    llarp_time_t m_LastWarn = 0s;

   protected:
    /// flag for PathSet::Stop()
    std::atomic<bool> _run;

    virtual bool
    UrgentBuild(llarp_time_t now) const;

    /// return true if we hit our soft limit for building paths too fast on a first hop
    bool
    BuildCooldownHit(RouterID edge) const;

   private:
    void
    DoPathBuildBackoff();

    void
    setup_hop_keys(path::PathHopConfig& hop, const RouterID& nextHop);

    std::string
    create_hop_info_frame(const path::PathHopConfig& hop);

   public:
    Router* const router;
    size_t numHops;
    llarp_time_t lastBuild = 0s;
    llarp_time_t buildIntervalLimit = MIN_PATH_BUILD_INTERVAL;

    /// construct
    Builder(Router* p_router, size_t numDesiredPaths, size_t numHops);

    virtual ~Builder() = default;

    util::StatusObject
    ExtractStatus() const;

    bool
    ShouldBuildMore(llarp_time_t now) const override;

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

    BuildStats
    CurrentBuildStats() const
    {
      return m_BuildStats;
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

    std::optional<std::vector<RemoteRC>>
    GetHopsAlignedToForBuild(RouterID endpoint, const std::set<RouterID>& exclude = {});

    void
    Build(std::vector<RemoteRC> hops, PathRole roles = ePathRoleAny) override;

    /// pick a first hop
    std::optional<RemoteRC>
    SelectFirstHop(const std::set<RouterID>& exclude = {}) const;

    std::optional<std::vector<RemoteRC>>
    GetHopsForBuild() override;

    void
    ManualRebuild(size_t N, PathRole roles = ePathRoleAny);

    void
    HandlePathBuilt(Path_ptr p) override;

    void
    HandlePathBuildTimeout(Path_ptr p) override;

    void
    HandlePathBuildFailedAt(Path_ptr p, RouterID hop) override;
  };

  using Builder_ptr = std::shared_ptr<Builder>;

}  // namespace llarp::path
