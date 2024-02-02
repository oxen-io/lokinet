#pragma once

#include "address.hpp"
#include "endpoint_types.hpp"
#include "lns_tracker.hpp"
#include "pendingbuffer.hpp"
#include "router_lookup_job.hpp"
#include "session.hpp"

#include <llarp/router_id.hpp>
#include <llarp/util/compare_ptr.hpp>
#include <llarp/util/decaying_hashtable.hpp>
#include <llarp/util/status.hpp>

#include <oxenc/variant.h>

#include <memory>
#include <queue>
#include <set>
#include <unordered_map>

namespace llarp::service
{
    struct EndpointState
    {
        std::set<RouterID> snode_blacklist;

        Router* router;
        std::string key_file;
        std::string name;
        std::string net_NS;
        bool is_exit_enabled = false;

        PendingTrafficMap pending_traffic;

        ConnectionMap remote_sessions;
        ConnectionMap dead_sessions;

        std::set<ConvoTag> inbound_convotags;

        SNodeConnectionMap snode_sessions;

        std::unordered_multimap<Address, EnsurePathCallback> pending_service_lookups;
        std::unordered_map<Address, llarp_time_t> last_service_lookup_time;

        std::unordered_map<RouterID, uint32_t> service_lookup_fails;

        PendingRoutersMap pending_routers;

        llarp_time_t last_publish = 0s;
        llarp_time_t last_publish_attempt = 0s;
        /// our introset
        IntroSet local_introset;
        /// on initialize functions
        std::list<std::function<bool(void)>> on_init_callbacks;

        /// conversations
        std::unordered_map<ConvoTag, Session> m_Sessions;

        std::unordered_set<Address> m_OutboundSessions;

        util::DecayingHashTable<std::string, std::variant<Address, RouterID>, std::hash<std::string>> nameCache;

        LNSLookupTracker lnsTracker;

        bool Configure(const NetworkConfig& conf);

        util::StatusObject ExtractStatus(util::StatusObject& obj) const;
    };
}  // namespace llarp::service
