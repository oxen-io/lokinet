#pragma once

#include "endpoint_types.hpp"

namespace llarp::service
{
  struct EndpointUtil
  {
    static void
    ExpireSNodeSessions(llarp_time_t now, SNodeSessions& sessions);

    static void
    ExpirePendingTx(llarp_time_t now, PendingLookups& lookups);

    static void
    ExpirePendingRouterLookups(llarp_time_t now, PendingRouters& routers);

    static void
    DeregisterDeadSessions(llarp_time_t now, Sessions& sessions);

    static void
    TickRemoteSessions(
        llarp_time_t now, Sessions& remoteSessions, Sessions& deadSessions, ConvoMap& sessions);

    static void
    ExpireConvoSessions(llarp_time_t now, ConvoMap& sessions);

    static void
    StopRemoteSessions(Sessions& remoteSessions);

    static void
    StopSnodeSessions(SNodeSessions& sessions);

    static bool
    HasPathToService(const Address& addr, const Sessions& remoteSessions);

    static bool
    GetConvoTagsForService(const ConvoMap& sessions, const Address& addr, std::set<ConvoTag>& tags);
  };

  template <typename Endpoint_t>
  static path::Path::UniqueEndpointSet_t
  GetManyPathsWithUniqueEndpoints(
      Endpoint_t* ep,
      size_t N,
      std::optional<dht::Key_t> maybeLocation = std::nullopt,
      size_t tries = 10)
  {
    std::unordered_set<RouterID> exclude;
    path::Path::UniqueEndpointSet_t paths;
    do
    {
      --tries;
      path::Path_ptr path;
      if (maybeLocation)
      {
        path = ep->GetEstablishedPathClosestTo(RouterID{maybeLocation->as_array()}, exclude);
      }
      else
      {
        path = ep->PickRandomEstablishedPath();
      }
      if (path and path->IsReady())
      {
        paths.emplace(path);
        exclude.insert(path->Endpoint());
      }
    } while (tries > 0 and paths.size() < N);
    return paths;
  }
}  // namespace llarp::service
