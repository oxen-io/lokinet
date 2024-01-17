#pragma once

#include "path_types.hpp"

#include <llarp/router_contact.hpp>
#include <llarp/router_id.hpp>
#include <llarp/service/intro_set.hpp>
#include <llarp/service/protocol_type.hpp>
#include <llarp/util/status.hpp>
#include <llarp/util/thread/threading.hpp>
#include <llarp/util/time.hpp>

#include <functional>
#include <list>
#include <map>
#include <tuple>
#include <unordered_set>

namespace std
{
  template <>
  struct hash<std::pair<llarp::RouterID, llarp::PathID_t>>
  {
    size_t
    operator()(const std::pair<llarp::RouterID, llarp::PathID_t>& i) const
    {
      return hash<llarp::RouterID>{}(i.first) ^ hash<llarp::PathID_t>{}(i.second);
    }
  };
}  // namespace std

namespace llarp
{
  struct Router;
  struct RouterContact;
  class NodeDB;

  namespace path
  {
    /// status of a path
    enum class PathStatus
    {
      BUILDING,
      ESTABLISHED,
      TIMEOUT,
      FAILED,
      IGNORE,
      EXPIRED
    };

    /// Stats about all our path builds
    struct BuildStats
    {
      static constexpr double MinGoodRatio = 0.25;

      uint64_t attempts = 0;
      uint64_t success = 0;
      uint64_t fails = 0;
      uint64_t timeouts = 0;

      util::StatusObject
      ExtractStatus() const;

      double
      SuccessRatio() const;

      std::string
      ToString() const;
    };

    /// the role of this path can fulfill
    enum class Path_Role
    {
      ANY = 0,
      EXIT = 1 << 1,
      SERVICE = 1 << 2
    };

    using PathRole = int;

    /// capable of any role
    constexpr PathRole ePathRoleAny = 0;
    /// exit traffic capable
    constexpr PathRole ePathRoleExit = (1 << 1);
    /// service node capable
    constexpr PathRole ePathRoleSVC = (1 << 2);

    // forward declare
    struct Path;

    /// a set of paths owned by an entity
    struct PathSet
    {
      /// maximum number of paths a path set can maintain
      // static constexpr size_t max_paths = 32;
      /// construct
      /// @params numDesiredPaths the number of paths to maintain
      PathSet(size_t numDesiredPaths);

      /// get a shared_ptr of ourself
      virtual std::shared_ptr<PathSet>
      GetSelf() = 0;

      /// get a weak_ptr of ourself
      virtual std::weak_ptr<PathSet>
      GetWeak() = 0;

      virtual void
      BuildOne(PathRole roles = ePathRoleAny) = 0;

      /// manual build on these hops
      virtual void
      Build(std::vector<RemoteRC> hops, PathRole roles = ePathRoleAny) = 0;

      /// tick owned paths
      virtual void
      Tick(llarp_time_t now);

      /// count the number of paths that will exist at this timestamp in future
      size_t
      NumPathsExistingAt(llarp_time_t futureTime) const;

      virtual void
      HandlePathBuilt(std::shared_ptr<Path> path) = 0;

      virtual void
      HandlePathBuildTimeout(std::shared_ptr<Path> path);

      virtual void
      HandlePathBuildFailedAt(std::shared_ptr<Path> path, RouterID hop);

      void
      PathBuildStarted(std::shared_ptr<Path> path);

      /// a path died now what?
      virtual void
      HandlePathDied(std::shared_ptr<Path> path);

      bool
      GetNewestIntro(service::Introduction& intro) const;

      void
      AddPath(std::shared_ptr<Path> path);

      std::shared_ptr<Path>
      GetByUpstream(RouterID remote, PathID_t rxid) const;

      void
      ExpirePaths(llarp_time_t now, Router* router);

      /// get the number of paths in this status
      size_t
      NumInStatus(PathStatus st) const;

      /// get the number of paths that match the role that are available
      size_t
      AvailablePaths(PathRole role) const;

      /// get time from event loop
      virtual llarp_time_t
      Now() const = 0;

      /// stop this pathset and mark it as to be removed
      virtual bool
      Stop() = 0;

      /// return true if we are stopped
      virtual bool
      IsStopped() const = 0;

      /// get the "name" of this pathset
      virtual std::string
      Name() const = 0;

      /// return true if we can and should remove this pathset and underlying
      /// resources from its parent context
      virtual bool
      ShouldRemove() const = 0;

      /// return true if we should build another path
      virtual bool
      ShouldBuildMore(llarp_time_t now) const;

      /// return true if we need another path with the given path roles
      // virtual bool
      // ShouldBuildMoreForRoles(llarp_time_t now, PathRole roles) const;

      /// return the minimum number of paths we want for given roles
      // virtual size_t
      // MinRequiredForRoles(PathRole roles) const;

      /// return true if we should publish a new hidden service descriptor
      virtual bool
      ShouldPublishDescriptors([[maybe_unused]] llarp_time_t now) const
      {
        return false;
      }

      virtual void
      BlacklistSNode(const RouterID) = 0;

      std::shared_ptr<Path>
      GetEstablishedPathClosestTo(
          RouterID router,
          std::unordered_set<RouterID> excluding = {},
          PathRole roles = ePathRoleAny) const;

      std::shared_ptr<Path>
      PickEstablishedPath(PathRole roles = ePathRoleAny) const;

      std::shared_ptr<Path>
      PickRandomEstablishedPath(PathRole roles = ePathRoleAny) const;

      std::shared_ptr<Path>
      GetPathByRouter(RouterID router, PathRole roles = ePathRoleAny) const;

      std::shared_ptr<Path>
      GetNewestPathByRouter(RouterID router, PathRole roles = ePathRoleAny) const;

      std::shared_ptr<Path>
      GetRandomPathByRouter(RouterID router, PathRole roles = ePathRoleAny) const;

      std::shared_ptr<Path>
      GetPathByID(PathID_t id) const;

      std::shared_ptr<Path>
      GetByEndpointWithID(RouterID router, PathID_t id) const;

      std::optional<std::set<service::Introduction>>
      GetCurrentIntroductionsWithFilter(
          std::function<bool(const service::Introduction&)> filter) const;

      /// reset all cooldown timers
      virtual void
      ResetInternalState() = 0;

      virtual bool
      BuildOneAlignedTo(const RouterID endpoint) = 0;

      virtual void
      send_packet_to_remote(std::string buf) = 0;

      virtual std::optional<std::vector<RemoteRC>>
      GetHopsForBuild() = 0;

      void
      ForEachPath(std::function<void(const std::shared_ptr<Path>&)> visit) const
      {
        Lock_t lock(paths_mutex);
        auto itr = _paths.begin();
        while (itr != _paths.end())
        {
          visit(itr->second);
          ++itr;
        }
      }

      void
      UpstreamFlush(Router* r);

      void
      DownstreamFlush(Router* r);

      size_t num_paths_desired;

     protected:
      BuildStats build_stats;

      void
      TickPaths(Router* r);

      using Mtx_t = util::NullMutex;
      using Lock_t = util::NullLock;
      mutable Mtx_t paths_mutex;

      std::unordered_map<std::pair<RouterID, PathID_t>, std::shared_ptr<Path>> _paths;

     private:
      std::unordered_map<RouterID, std::weak_ptr<path::Path>> path_cache;
    };

  }  // namespace path

  template <>
  constexpr inline bool IsToStringFormattable<path::BuildStats> = true;

}  // namespace llarp
