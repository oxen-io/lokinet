#ifndef LLARP_SERVICE_ENDPOINT_UTIL_HPP
#define LLARP_SERVICE_ENDPOINT_UTIL_HPP

#include <service/endpoint_types.hpp>

namespace llarp
{
  namespace service
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
      GetConvoTagsForService(
          const ConvoMap& sessions, const Address& addr, std::set<ConvoTag>& tags);
    };

    template <typename Endpoint_t>
    static path::Path::UniqueEndpointSet_t
    GetManyPathsWithUniqueEndpoints(Endpoint_t* ep, size_t N, size_t tries = 10)
    {
      path::Path::UniqueEndpointSet_t paths;
      do
      {
        --tries;
        const auto path = ep->PickRandomEstablishedPath();
        if (path)
          paths.emplace(path);
      } while (tries > 0 and paths.size() < N);
      return paths;
    }
  }  // namespace service

}  // namespace llarp

#endif
