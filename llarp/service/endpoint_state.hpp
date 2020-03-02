#ifndef LLARP_SERVICE_ENDPOINT_STATE_HPP
#define LLARP_SERVICE_ENDPOINT_STATE_HPP

#include <hook/ihook.hpp>
#include <router_id.hpp>
#include <service/address.hpp>
#include <service/pendingbuffer.hpp>
#include <service/router_lookup_job.hpp>
#include <service/session.hpp>
#include <service/tag_lookup_job.hpp>
#include <service/endpoint_types.hpp>
#include <util/compare_ptr.hpp>
#include <util/status.hpp>

#include <memory>
#include <queue>
#include <set>
#include <unordered_map>

struct llarp_ev_loop;
using llarp_ev_loop_ptr = std::shared_ptr< llarp_ev_loop >;

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

      util::Mutex m_InboundTrafficQueueMutex;  // protects m_InboundTrafficQueue
      /// ordered queue for inbound hidden service traffic
      RecvPacketQueue_t m_InboundTrafficQueue
          GUARDED_BY(m_InboundTrafficQueueMutex);

      std::set< RouterID > m_SnodeBlacklist;

      AbstractRouter* m_Router;
      std::shared_ptr< Logic > m_IsolatedLogic = nullptr;
      llarp_ev_loop_ptr m_IsolatedNetLoop      = nullptr;
      std::string m_Keyfile;
      std::string m_Name;
      std::string m_NetNS;
      bool m_BundleRC = false;

      util::Mutex m_SendQueueMutex;  // protects m_SendQueue
      std::deque< SendEvent_t > m_SendQueue GUARDED_BY(m_SendQueueMutex);

      PendingTraffic m_PendingTraffic;

      Sessions m_RemoteSessions;
      Sessions m_DeadSessions;

      std::set< ConvoTag > m_InboundConvos;

      SNodeSessions m_SNodeSessions;

      std::unordered_multimap< Address, PathEnsureHook, Address::Hash >
          m_PendingServiceLookups;

      std::unordered_map< RouterID, uint32_t, RouterID::Hash >
          m_ServiceLookupFails;

      PendingRouters m_PendingRouters;

      llarp_time_t m_LastPublish        = 0s;
      llarp_time_t m_LastPublishAttempt = 0s;
      llarp_time_t m_MinPathLatency     = 1s;
      /// our introset
      IntroSet m_IntroSet;
      /// pending remote service lookups by id
      PendingLookups m_PendingLookups;
      /// prefetch remote address list
      std::set< Address > m_PrefetchAddrs;
      /// hidden service tag
      Tag m_Tag;
      /// prefetch descriptors for these hidden service tags
      std::set< Tag > m_PrefetchTags;
      /// on initialize functions
      std::list< std::function< bool(void) > > m_OnInit;

      /// conversations
      ConvoMap m_Sessions;

      OutboundSessions_t m_OutboundSessions;

      std::unordered_map< Tag, CachedTagResult, Tag::Hash > m_PrefetchedTags;

      bool
      SetOption(const std::string& k, const std::string& v, Endpoint& ep);

      util::StatusObject
      ExtractStatus(util::StatusObject& obj) const;
    };
  }  // namespace service
}  // namespace llarp

#endif
