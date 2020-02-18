#ifndef LLARP_SERVICE_ENDPOINT_TYPES_HPP
#define LLARP_SERVICE_ENDPOINT_TYPES_HPP

#include <service/pendingbuffer.hpp>
#include <service/router_lookup_job.hpp>
#include <service/session.hpp>
#include <util/compare_ptr.hpp>

#include <deque>
#include <memory>
#include <queue>
#include <unordered_map>

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

    using Msg_ptr     = std::shared_ptr< const routing::PathTransferMessage >;
    using SendEvent_t = std::pair< Msg_ptr, path::Path_ptr >;
    using PendingBufferQueue = std::deque< PendingBuffer >;
    using PendingTraffic =
        std::unordered_map< Address, PendingBufferQueue, Address::Hash >;

    using ProtocolMessagePtr = std::shared_ptr< ProtocolMessage >;
    using RecvPacketQueue_t =
        std::priority_queue< ProtocolMessagePtr,
                             std::vector< ProtocolMessagePtr >,
                             ComparePtr< ProtocolMessagePtr > >;

    using PendingRouters =
        std::unordered_map< RouterID, RouterLookupJob, RouterID::Hash >;

    using PendingLookups =
        std::unordered_map< uint64_t, std::unique_ptr< IServiceLookup > >;

    using Sessions =
        std::unordered_multimap< Address, std::shared_ptr< OutboundContext >,
                                 Address::Hash >;

    using SNodeSessionValue =
        std::pair< std::shared_ptr< exit::BaseSession >, ConvoTag >;

    using SNodeSessions =
        std::unordered_multimap< RouterID, SNodeSessionValue, RouterID::Hash >;

    using ConvoMap = std::unordered_map< ConvoTag, Session, ConvoTag::Hash >;

    /// set of outbound addresses to maintain to
    using OutboundSessions_t = std::unordered_set< Address, Address::Hash >;

    using PathEnsureHook = std::function< void(Address, OutboundContext*) >;

  }  // namespace service
}  // namespace llarp

#endif
