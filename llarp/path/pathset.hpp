#pragma once

#include "path_types.hpp"
#include <llarp/router_id.hpp>
#include <llarp/routing/message.hpp>
#include <llarp/service/intro_set.hpp>
#include <llarp/util/status.hpp>
#include <llarp/util/thread/threading.hpp>
#include <llarp/util/time.hpp>

#include <functional>
#include <list>
#include <map>
#include <tuple>

namespace llarp
{
  struct RouterContact;
  class NodeDB;

  namespace dht
  {
    struct GotIntroMessage;
    struct GotRouterMessage;
    struct GotNameMessage;
  }  // namespace dht

  namespace path
  {
    /// status of a path
    enum PathStatus
    {
      ePathBuilding,
      ePathEstablished,
      ePathTimeout,
      ePathFailed,
      ePathIgnore,
      ePathExpired
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

      friend std::ostream&
      operator<<(std::ostream& o, const BuildStats& st)
      {
        return o << st.ToString();
      }
    };

    /// the role of this path can fulfill
    using PathRole = int;

    /// capable of any role
    constexpr PathRole ePathRoleAny = 0;
    /// outbound hs traffic capable
    constexpr PathRole ePathRoleOutboundHS = (1 << 0);
    /// inbound hs traffic capable
    constexpr PathRole ePathRoleInboundHS = (1 << 1);
    /// exit traffic capable
    constexpr PathRole ePathRoleExit = (1 << 2);
    /// service node capable
    constexpr PathRole ePathRoleSVC = (1 << 3);
    /// dht message capable
    constexpr PathRole ePathRoleDHT = (1 << 4);

    // forward declare
    struct Path;

    using Path_ptr = std::shared_ptr<Path>;

    struct PathSet;

    using PathSet_ptr = std::shared_ptr<PathSet>;

    /// a set of paths owned by an entity
    struct PathSet
    {
      /// maximum number of paths a path set can maintain
      static constexpr size_t max_paths = 32;
      /// construct
      /// @params numDesiredPaths the number of paths to maintain
      PathSet(size_t numDesiredPaths);

      /// get a shared_ptr of ourself
      virtual PathSet_ptr
      GetSelf() = 0;

      virtual void
      BuildOne(PathRole roles = ePathRoleAny) = 0;

      /// manual build on these hops
      virtual void
      Build(std::vector<RouterContact> hops, PathRole roles = ePathRoleAny) = 0;

      /// tick owned paths
      virtual void
      Tick(llarp_time_t now) = 0;

      /// count the number of paths that will exist at this timestamp in future
      size_t
      NumPathsExistingAt(llarp_time_t futureTime) const;

      virtual void
      HandlePathBuilt(Path_ptr path) = 0;

      virtual void
      HandlePathBuildTimeout(Path_ptr path);

      virtual void
      HandlePathBuildFailed(Path_ptr path);

      virtual void
      PathBuildStarted(Path_ptr path);

      /// a path died now what?
      virtual void
      HandlePathDied(Path_ptr path);

      bool
      GetNewestIntro(service::Introduction& intro) const;

      void
      AddPath(Path_ptr path);

      Path_ptr
      GetByUpstream(RouterID remote, PathID_t rxid) const;

      void
      ExpirePaths(llarp_time_t now, AbstractRouter* router);

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
      virtual bool
      ShouldBuildMoreForRoles(llarp_time_t now, PathRole roles) const;

      /// return the minimum number of paths we want for given roles
      virtual size_t
      MinRequiredForRoles(PathRole roles) const;

      /// return true if we should publish a new hidden service descriptor
      virtual bool
      ShouldPublishDescriptors([[maybe_unused]] llarp_time_t now) const
      {
        return false;
      }

      virtual void
      BlacklistSNode(const RouterID) = 0;

      /// override me in subtype
      virtual bool HandleGotIntroMessage(std::shared_ptr<const dht::GotIntroMessage>)
      {
        return false;
      }

      /// override me in subtype
      virtual bool HandleGotRouterMessage(std::shared_ptr<const dht::GotRouterMessage>)
      {
        return false;
      }

      /// override me in subtype
      virtual bool HandleGotNameMessage(std::shared_ptr<const dht::GotNameMessage>)
      {
        return false;
      }

      virtual routing::IMessageHandler*
      GetDHTHandler()
      {
        return nullptr;
      }

      Path_ptr
      GetEstablishedPathClosestTo(RouterID router, PathRole roles = ePathRoleAny) const;

      Path_ptr
      PickRandomEstablishedPath(PathRole roles = ePathRoleAny) const;

      Path_ptr
      GetPathByRouter(RouterID router, PathRole roles = ePathRoleAny) const;

      Path_ptr
      GetNewestPathByRouter(RouterID router, PathRole roles = ePathRoleAny) const;

      Path_ptr
      GetRandomPathByRouter(RouterID router, PathRole roles = ePathRoleAny) const;

      Path_ptr
      GetPathByID(PathID_t id) const;

      Path_ptr
      GetByEndpointWithID(RouterID router, PathID_t id) const;

      bool
      GetCurrentIntroductionsWithFilter(
          std::set<service::Introduction>& intros,
          std::function<bool(const service::Introduction&)> filter) const;

      bool
      GetCurrentIntroductions(std::set<service::Introduction>& intros) const;

      virtual bool
      PublishIntroSet(const service::EncryptedIntroSet&, AbstractRouter*)
      {
        return false;
      }

      /// reset all cooldown timers
      virtual void
      ResetInternalState() = 0;

      virtual bool
      BuildOneAlignedTo(const RouterID endpoint) = 0;

      virtual void
      SendPacketToRemote(const llarp_buffer_t& pkt) = 0;

      virtual std::optional<std::vector<RouterContact>>
      GetHopsForBuild() = 0;

      void
      ForEachPath(std::function<void(const Path_ptr&)> visit) const
      {
        Lock_t lock(m_PathsMutex);
        auto itr = m_Paths.begin();
        while (itr != m_Paths.end())
        {
          visit(itr->second);
          ++itr;
        }
      }

      void
      UpstreamFlush(AbstractRouter* r);

      void
      DownstreamFlush(AbstractRouter* r);

      size_t numDesiredPaths;

     protected:
      BuildStats m_BuildStats;

      void
      TickPaths(AbstractRouter* r);

      using PathInfo_t = std::pair<RouterID, PathID_t>;

      struct PathInfoHash
      {
        size_t
        operator()(const PathInfo_t& i) const
        {
          return RouterID::Hash()(i.first) ^ PathID_t::Hash()(i.second);
        }
      };

      struct PathInfoEquals
      {
        bool
        operator()(const PathInfo_t& left, const PathInfo_t& right) const
        {
          return left.first == right.first && left.second == right.second;
        }
      };

      using Mtx_t = util::NullMutex;
      using Lock_t = util::NullLock;
      using PathMap_t = std::unordered_map<PathInfo_t, Path_ptr, PathInfoHash, PathInfoEquals>;
      mutable Mtx_t m_PathsMutex;
      PathMap_t m_Paths;
    };

  }  // namespace path
}  // namespace llarp
