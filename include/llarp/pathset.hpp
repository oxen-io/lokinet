#ifndef LLARP_PATHSET_HPP
#define LLARP_PATHSET_HPP
#include <llarp/time.h>
#include <functional>
#include <list>
#include <llarp/path_types.hpp>
#include <llarp/router_id.hpp>
#include <llarp/service/IntroSet.hpp>
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
      bool
      ShouldPublishDescriptors() const;

      virtual bool
      HandleGotIntroMessage(const llarp::dht::GotIntroMessage* msg)
      {
        return false;
      }

      Path*
      PickRandomEstablishedPath();

      bool
      GetCurrentIntroductions(
          std::list< llarp::service::Introduction >& intros) const;

      bool
      PublishIntroSet(const llarp::service::IntroSet& introset,
                      llarp_router* r);

      typedef std::function< void(const llarp::service::IntroSet*) >
          ServiceLookupHandler;

      /// return false if we are already pending a lookup for this address
      bool
      LookupService(const llarp::service::Address& addr,
                    ServiceLookupHandler handler);

     protected:
      void
      IssueServiceLookup(const llarp::service::Address& addr);

     private:
      typedef std::pair< RouterID, PathID_t > PathInfo_t;
      typedef std::map< PathInfo_t, Path* > PathMap_t;

      size_t m_NumPaths;
      PathMap_t m_Paths;
      uint64_t m_CurrentPublishTX = 0;
      std::unordered_map< llarp::service::Address, ServiceLookupHandler,
                          llarp::service::Address::Hash >
          m_ServiceLookups;
    };

  }  // namespace path
}  // namespace llarp

#endif