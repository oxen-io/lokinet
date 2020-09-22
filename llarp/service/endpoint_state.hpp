#ifndef LLARP_SERVICE_ENDPOINT_STATE_HPP
#define LLARP_SERVICE_ENDPOINT_STATE_HPP

#include <hook/ihook.hpp>
#include <router_id.hpp>
#include <service/address.hpp>
#include <service/pendingbuffer.hpp>
#include <service/router_lookup_job.hpp>
#include <service/session.hpp>
#include <service/endpoint_types.hpp>
#include <util/compare_ptr.hpp>
#include <util/decaying_hashtable.hpp>
#include <util/status.hpp>

#include <memory>
#include <queue>
#include <set>
#include <unordered_map>

struct llarp_ev_loop;
using llarp_ev_loop_ptr = std::shared_ptr<llarp_ev_loop>;

namespace llarp
{
  // clang-format off
  namespace exit { struct BaseSession; }
  namespace path { struct Path; using Path_ptr = std::shared_ptr< Path >; }
  namespace routing { struct PathTransferMessage; }
  // clang-format on

  namespace service
  {
    struct IServiceLookup;
    struct OutboundContext;
    struct Endpoint;

    struct EndpointState
    {
      hooks::Backend_ptr m_OnUp;
      hooks::Backend_ptr m_OnDown;
      hooks::Backend_ptr m_OnReady;

      std::set<RouterID> m_SnodeBlacklist;

      AbstractRouter* m_Router;
      std::shared_ptr<Logic> m_IsolatedLogic = nullptr;
      llarp_ev_loop_ptr m_IsolatedNetLoop = nullptr;
      std::string m_Keyfile;
      std::string m_Name;
      std::string m_NetNS;
      bool m_ExitEnabled = false;

      PendingTraffic m_PendingTraffic;

      Sessions m_RemoteSessions;
      Sessions m_DeadSessions;

      std::set<ConvoTag> m_InboundConvos;

      SNodeSessions m_SNodeSessions;

      std::unordered_multimap<Address, PathEnsureHook, Address::Hash> m_PendingServiceLookups;
      std::unordered_map<Address, llarp_time_t, Address::Hash> m_LastServiceLookupTimes;

      std::unordered_map<RouterID, uint32_t, RouterID::Hash> m_ServiceLookupFails;

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

      util::DecayingHashTable<std::string, Address, std::hash<std::string>> nameCache;

      bool
      Configure(const NetworkConfig& conf);

      util::StatusObject
      ExtractStatus(util::StatusObject& obj) const;
    };
  }  // namespace service
}  // namespace llarp

#endif
