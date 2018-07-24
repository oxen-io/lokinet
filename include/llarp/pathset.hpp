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

      void
      HandlePathBuilt(Path* path);

      void
      AddPath(Path* path);

      Path*
      GetByUpstream(const RouterID& remote, const PathID_t& rxid);

      void
      ExpirePaths(llarp_time_t now);

      size_t
      NumInStatus(PathStatus st) const;

      /// return true if we should build another path
      bool
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

      Path*
      PickRandomEstablishedPath();

      Path*
      GetPathByRouter(const RouterID& router);

      bool
      GetCurrentIntroductions(
          std::set< llarp::service::Introduction >& intros) const;

      virtual bool
      PublishIntroSet(llarp_router* r)
      {
        return false;
      }

      virtual bool
      SelectHop(llarp_nodedb* db, llarp_rc* prev, llarp_rc* cur,
                size_t hop) = 0;

      static void
      SelectHopCallback(void* user, llarp_nodedb* db, llarp_rc* prev,
                        llarp_rc* cur, size_t hopno)
      {
        PathSet* self = static_cast< PathSet* >(user);
        self->SelectHop(db, prev, cur, hopno);
      }

     private:
      typedef std::pair< RouterID, PathID_t > PathInfo_t;
      typedef std::map< PathInfo_t, Path* > PathMap_t;
      size_t m_NumPaths;
      PathMap_t m_Paths;
    };

  }  // namespace path
}  // namespace llarp

#endif