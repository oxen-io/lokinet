#pragma once

#include "pendingbuffer.hpp"
#include "router_lookup_job.hpp"
#include "session.hpp"
#include <llarp/util/compare_ptr.hpp>
#include <llarp/util/thread/queue.hpp>

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

    using Msg_ptr = std::shared_ptr<const routing::PathTransferMessage>;

    using SendEvent_t = std::pair<Msg_ptr, path::Path_ptr>;
    using SendMessageQueue_t = thread::Queue<SendEvent_t>;

    using PendingBufferQueue = std::deque<PendingBuffer>;
    using PendingTraffic = std::unordered_map<Address, PendingBufferQueue>;

    using ProtocolMessagePtr = std::shared_ptr<ProtocolMessage>;
    using RecvPacketQueue_t = thread::Queue<ProtocolMessagePtr>;

    using PendingRouters = std::unordered_map<RouterID, RouterLookupJob>;

    using PendingLookups = std::unordered_map<uint64_t, std::unique_ptr<IServiceLookup>>;

    using Sessions = std::unordered_multimap<Address, std::shared_ptr<OutboundContext>>;

    using SNodeSessionValue = std::pair<std::shared_ptr<exit::BaseSession>, ConvoTag>;

    using SNodeSessions = std::unordered_multimap<RouterID, SNodeSessionValue>;

    using ConvoMap = std::unordered_map<ConvoTag, Session>;

    /// set of outbound addresses to maintain to
    using OutboundSessions_t = std::unordered_set<Address>;

    using PathEnsureHook = std::function<void(Address, OutboundContext*)>;

    using LNSNameCache = std::unordered_map<std::string, std::pair<Address, llarp_time_t>>;

  }  // namespace service
}  // namespace llarp
