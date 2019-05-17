#ifndef LLARP_SERVICE_ENDPOINT_UTIL_HPP
#define LLARP_SERVICE_ENDPOINT_UTIL_HPP

#include <service/endpoint.hpp>

namespace llarp
{
  namespace service
  {
    struct EndpointUtil
    {
      static void
      ExpireSNodeSessions(llarp_time_t now, Endpoint::SNodeSessions& sessions);

      static void
      ExpirePendingTx(llarp_time_t now, Endpoint::PendingLookups& lookups);

      static void
      ExpirePendingRouterLookups(llarp_time_t now,
                                 Endpoint::PendingRouters& routers);

      static void
      DeregisterDeadSessions(llarp_time_t now, Endpoint::Sessions& sessions);

      static void
      TickRemoteSessions(llarp_time_t now, Endpoint::Sessions& remoteSessions,
                         Endpoint::Sessions& deadSessions);

      static void
      ExpireConvoSessions(llarp_time_t now, Endpoint::ConvoMap& sessions);

      static void
      StopRemoteSessions(Endpoint::Sessions& remoteSessions);

      static void
      StopSnodeSessions(Endpoint::SNodeSessions& sessions);

      static bool
      HasPathToService(const Address& addr,
                       const Endpoint::Sessions& remoteSessions);

      static bool
      GetConvoTagsForService(const Endpoint::ConvoMap& sessions,
                             const ServiceInfo& info,
                             std::set< ConvoTag >& tags);
    };
  }  // namespace service

}  // namespace llarp

#endif
