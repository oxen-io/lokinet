#ifndef LLARP_PATHSET_HPP
#define LLARP_PATHSET_HPP
#include <llarp/time.h>
#include <functional>
#include <list>
#include <llarp/path_types.hpp>
#include <llarp/router_id.hpp>
#include <llarp/routing/message.hpp>
#include <llarp/service/IntroSet.hpp>
#include <llarp/service/lookup.hpp>
#include <llarp/dht/messages/all.hpp>
#include <map>
#include <tuple>

namespace llarp
{
  namespace dht
  {
    struct GotIntroMessage;
  }

  namespace path
  {
    enum PathStatus
    {
      ePathBuilding,
      ePathEstablished,
      ePathTimeout,
      ePathExpired
    };
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
      Tick(llarp_time_t now, llarp_router* r);

      void
      RemovePath(Path* path);

      virtual void
      HandlePathBuilt(Path* path);

      virtual void
      HandlePathBuildTimeout(Path* path);

      void
      AddPath(Path* path);

      Path*
      GetByUpstream(const RouterID& remote, const PathID_t& rxid) const;

      void
      ExpirePaths(llarp_time_t now);

      size_t
      NumInStatus(PathStatus st) const;

      /// return true if we should build another path
      virtual bool
      ShouldBuildMore() const;

      /// return true if we should publish a new hidden service descriptor
      virtual bool
      ShouldPublishDescriptors(llarp_time_t now) const
      {
        (void)now;
        return false;
      }

      /// override me in subtype
      virtual bool
      HandleGotIntroMessage(const llarp::dht::GotIntroMessage* msg)
      {
        return false;
      }

      /// override me in subtype
      virtual bool
      HandleGotRouterMessage(const llarp::dht::GotRouterMessage* msg)
      {
        return false;
      }

      virtual routing::IMessageHandler*
      GetDHTHandler()
      {
        return nullptr;
      }

      Path*
      GetEstablishedPathClosestTo(const RouterID& router) const;

      Path*
      PickRandomEstablishedPath() const;

      Path*
      GetPathByRouter(const RouterID& router) const;

      Path*
      GetNewestPathByRouter(const RouterID& router) const;

      Path*
      GetPathByID(const PathID_t& id) const;

      bool
      GetCurrentIntroductions(
          std::set< llarp::service::Introduction >& intros) const;

      virtual bool
      PublishIntroSet(llarp_router* r)
      {
        return false;
      }

      virtual bool
      SelectHop(llarp_nodedb* db, const RouterContact& prev, RouterContact& cur,
                size_t hop) = 0;

     private:
      typedef std::pair< RouterID, PathID_t > PathInfo_t;
      typedef std::map< PathInfo_t, Path* > PathMap_t;
      size_t m_NumPaths;
      PathMap_t m_Paths;
    };

  }  // namespace path
}  // namespace llarp

#endif
