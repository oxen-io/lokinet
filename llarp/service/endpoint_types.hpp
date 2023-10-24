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

    using Msg_ptr = std::shared_ptr<routing::PathTransferMessage>;

    using SendEvent = std::pair<Msg_ptr, path::Path_ptr>;
    using SendMessageEventQueue = thread::Queue<SendEvent>;

    using PendingBufferDeque = std::deque<PendingBuffer>;
    using PendingTrafficMap = std::unordered_map<Address, PendingBufferDeque>;

    using ProtocolMessagePtr = std::shared_ptr<ProtocolMessage>;
    using RecvPacketQueue_t = thread::Queue<ProtocolMessagePtr>;

    using PendingRoutersMap = std::unordered_map<RouterID, RouterLookupJob>;

    using PendingLookupsMap = std::unordered_map<uint64_t, std::unique_ptr<IServiceLookup>>;

    using ConnectionMap = std::unordered_multimap<Address, std::shared_ptr<OutboundContext>>;

    using SNodeConnectionMap = std::unordered_map<RouterID, std::shared_ptr<exit::BaseSession>>;

    using ConvoMap = std::unordered_map<ConvoTag, Session>;

    using EnsurePathCallback = std::function<void(Address, OutboundContext*)>;

    using ONSNameCache = std::unordered_map<std::string, std::pair<Address, llarp_time_t>>;

  }  // namespace service
}  // namespace llarp
