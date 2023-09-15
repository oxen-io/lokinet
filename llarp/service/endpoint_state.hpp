#pragma once

#include <llarp/router_id.hpp>
#include "address.hpp"
#include "pendingbuffer.hpp"
#include "router_lookup_job.hpp"
#include "session.hpp"
#include "endpoint_types.hpp"
#include <llarp/util/compare_ptr.hpp>
#include <llarp/util/decaying_hashtable.hpp>
#include <llarp/util/status.hpp>
#include "lns_tracker.hpp"

#include <memory>
#include <queue>
#include <set>
#include <unordered_map>

#include <oxenc/variant.h>

namespace llarp
{
  namespace service
  {
    struct EndpointState
    {
      std::set<RouterID> m_SnodeBlacklist;

      Router* m_Router;
      std::string m_Keyfile;
      std::string m_Name;
      std::string m_NetNS;
      bool m_ExitEnabled = false;

      PendingTraffic m_PendingTraffic;

      Sessions m_RemoteSessions;
      Sessions m_DeadSessions;

      std::set<ConvoTag> m_InboundConvos;

      SNodeSessions m_SNodeSessions;

      std::unordered_multimap<Address, PathEnsureHook> m_PendingServiceLookups;
      std::unordered_map<Address, llarp_time_t> m_LastServiceLookupTimes;

      std::unordered_map<RouterID, uint32_t> m_ServiceLookupFails;

      PendingRouters m_PendingRouters;

      llarp_time_t m_LastPublish = 0s;
      llarp_time_t m_LastPublishAttempt = 0s;
      /// our introset
      IntroSet m_IntroSet;
      /// pending remote service lookups by id
      PendingLookups m_PendingLookups;
      /// on initialize functions
      std::list<std::function<bool(void)>> m_OnInit;

      /// conversations
      ConvoMap m_Sessions;

      OutboundSessions_t m_OutboundSessions;

      util::DecayingHashTable<std::string, std::variant<Address, RouterID>, std::hash<std::string>>
          nameCache;

      LNSLookupTracker lnsTracker;

      bool
      Configure(const NetworkConfig& conf);

      util::StatusObject
      ExtractStatus(util::StatusObject& obj) const;
    };
  }  // namespace service
}  // namespace llarp
