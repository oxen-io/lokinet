#pragma once

#include "pathset.hpp"

#include <llarp/util/decaying_hashset.hpp>
#include <llarp/util/status.hpp>

#include <atomic>
#include <set>

namespace llarp::path
{
  // maximum number of paths a path-set can maintain
  inline constexpr size_t MAX_PATHS{32};

  /// limiter for path builds
  /// prevents overload and such
  class BuildLimiter
  {
    util::DecayingHashSet<RouterID> _edge_limiter;

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

  struct PathBuilder : public PathSet
  {
   private:
    llarp_time_t last_warn_time = 0s;
    // size_t _num_paths_desired;

   protected:
    /// flag for PathSet::Stop()
    std::atomic<bool> _run;

    BuildStats _build_stats;

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
    size_t num_hops;
    llarp_time_t _last_build = 0s;
    llarp_time_t build_interval_limit = MIN_PATH_BUILD_INTERVAL;

    /// construct
    PathBuilder(Router* p_router, size_t numDesiredPaths, size_t numHops);

    virtual ~PathBuilder() = default;

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
      return build_stats;
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
    HandlePathBuilt(std::shared_ptr<Path> p) override;

    void
    HandlePathBuildTimeout(std::shared_ptr<Path> p) override;

    void
    HandlePathBuildFailedAt(std::shared_ptr<Path> p, RouterID hop) override;
  };
}  // namespace llarp::path
