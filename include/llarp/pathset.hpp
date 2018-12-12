#ifndef LLARP_PATHSET_HPP
#define LLARP_PATHSET_HPP

#include <dht/messages/all.hpp>
#include <llarp/path_types.hpp>
#include <llarp/router_id.hpp>
#include <llarp/routing/message.hpp>
#include <llarp/service/IntroSet.hpp>
#include <llarp/service/lookup.hpp>
#include <llarp/time.hpp>

#include <functional>
#include <list>
#include <map>
#include <tuple>

struct llarp_nodedb;

namespace llarp
{
  namespace dht
  {
    struct GotIntroMessage;
  }

  namespace path
  {
    /// status of a path
    enum PathStatus
    {
      ePathBuilding,
      ePathEstablished,
      ePathTimeout,
      ePathExpired
    };

    /// the role of this path can fuffill
    using PathRole = int;

    /// capable of any role
    constexpr PathRole ePathRoleAny = 0;
    /// outbound hs traffic capabale
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

    /// a set of paths owned by an entity
    struct PathSet
    {
      /// construct
      /// @params numPaths the number of paths to maintain
      PathSet(size_t numPaths);

      /// tick owned paths
      void
      Tick(llarp_time_t now, llarp::Router* r);

      /// count the number of paths that will exist at this timestamp in future
      size_t
      NumPathsExistingAt(llarp_time_t futureTime) const;

      void
      RemovePath(Path* path);

      virtual void
      HandlePathBuilt(__attribute__((unused)) Path* path);

      virtual void
      HandlePathBuildTimeout(__attribute__((unused)) Path* path);

      bool
      GetNewestIntro(service::Introduction& intro) const;

      void
      AddPath(Path* path);

      Path*
      GetByUpstream(const RouterID& remote, const PathID_t& rxid) const;

      void
      ExpirePaths(llarp_time_t now);

      /// get the number of paths in this status
      size_t
      NumInStatus(PathStatus st) const;

      /// get the number of paths that match the role that are available
      size_t
      AvailablePaths(PathRole role) const;

      /// get time from event loop
      virtual llarp_time_t
      Now() const = 0;

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
      ShouldPublishDescriptors(__attribute__((unused)) llarp_time_t now) const
      {
        return false;
      }

      /// override me in subtype
      virtual bool
      HandleGotIntroMessage(__attribute__((unused))
                            const llarp::dht::GotIntroMessage* msg)
      {
        return false;
      }

      /// override me in subtype
      virtual bool
      HandleGotRouterMessage(__attribute__((unused))
                             const llarp::dht::GotRouterMessage* msg)
      {
        return false;
      }

      virtual routing::IMessageHandler*
      GetDHTHandler()
      {
        return nullptr;
      }

      Path*
      GetEstablishedPathClosestTo(const RouterID& router,
                                  PathRole roles = ePathRoleAny) const;

      Path*
      PickRandomEstablishedPath(PathRole roles = ePathRoleAny) const;

      Path*
      GetPathByRouter(const RouterID& router,
                      PathRole roles = ePathRoleAny) const;

      Path*
      GetNewestPathByRouter(const RouterID& router,
                            PathRole roles = ePathRoleAny) const;

      Path*
      GetPathByID(const PathID_t& id) const;

      bool
      GetCurrentIntroductionsWithFilter(
          std::set< llarp::service::Introduction >& intros,
          std::function< bool(const llarp::service::Introduction&) > filter)
          const;

      bool
      GetCurrentIntroductions(
          std::set< llarp::service::Introduction >& intros) const;

      virtual bool
      PublishIntroSet(__attribute__((unused)) llarp::Router* r)
      {
        return false;
      }

      virtual bool
      SelectHop(llarp_nodedb* db, const RouterContact& prev, RouterContact& cur,
                size_t hop, PathRole roles) = 0;

     protected:
      size_t m_NumPaths;

     private:
      using PathInfo_t = std::pair< RouterID, PathID_t >;

      struct PathInfoHash
      {
        size_t
        operator()(const PathInfo_t& i) const
        {
          return *i.first.data_l() ^ *i.second.data_l();
        }
      };
      using Mtx_t     = llarp::util::NullMutex;
      using Lock_t    = llarp::util::NullLock;
      using PathMap_t = std::unordered_map< PathInfo_t, Path*, PathInfoHash >;
      mutable Mtx_t m_PathsMutex;
      PathMap_t m_Paths;
    };

  }  // namespace path
}  // namespace llarp

#endif
