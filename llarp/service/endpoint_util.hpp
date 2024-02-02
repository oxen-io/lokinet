#pragma once

#include "endpoint_types.hpp"

namespace llarp::service
{
    struct EndpointUtil
    {
        static void ExpireSNodeSessions(llarp_time_t now, SNodeConnectionMap& sessions);

        static void ExpirePendingRouterLookups(llarp_time_t now, PendingRoutersMap& routers);

        static void DeregisterDeadSessions(llarp_time_t now, ConnectionMap& sessions);

        static void TickRemoteSessions(
            llarp_time_t now,
            ConnectionMap& remoteSessions,
            ConnectionMap& deadSessions,
            std::unordered_map<ConvoTag, Session>& sessions);

        static void ExpireConvoSessions(llarp_time_t now, std::unordered_map<ConvoTag, Session>& sessions);

        static void StopRemoteSessions(ConnectionMap& remoteSessions);

        static void StopSnodeSessions(SNodeConnectionMap& sessions);

        static bool HasPathToService(const Address& addr, const ConnectionMap& remoteSessions);

        static bool GetConvoTagsForService(
            const std::unordered_map<ConvoTag, Session>& sessions, const Address& addr, std::set<ConvoTag>& tags);
    };

    template <typename Endpoint_t>
    static std::unordered_set<std::shared_ptr<path::Path>, path::Endpoint_Hash, path::endpoint_comparator>
    GetManyPathsWithUniqueEndpoints(
        Endpoint_t* ep, size_t N, std::optional<dht::Key_t> maybeLocation = std::nullopt, size_t tries = 10)
    {
        std::unordered_set<RouterID> exclude;
        std::unordered_set<std::shared_ptr<path::Path>, path::Endpoint_Hash, path::endpoint_comparator> paths;
        do
        {
            --tries;
            std::shared_ptr<path::Path> path;
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
