#pragma once

#include <llarp/util/decaying_hashset.hpp>
#include "outbound_message_handler.hpp"
#include <llarp/link/link_manager.hpp>
#include "abstractrouter.hpp"

namespace llarp
{
  /// The maximum number of peers we will flood a gossiped RC to when propagating an RC
  constexpr size_t MaxGossipPeers = 20;

  struct RCGossiper
  {
    using Time_t = Duration_t;

    RCGossiper();

    ~RCGossiper() = default;

    bool
    GossipRC(const RouterContact& rc);

    void
    Decay(Time_t now);

    bool
    ShouldGossipOurRC(Time_t now) const;

    bool
    IsOurRC(const RouterContact& rc) const;

    void
    Init(LinkManager*, const RouterID&, AbstractRouter*);

    void
    Forget(const RouterID& router);

    TimePoint_t
    NextGossipAt() const;

    std::optional<TimePoint_t>
    LastGossipAt() const;

   private:
    RouterID m_OurRouterID;
    Time_t m_LastGossipedOurRC = 0s;
    LinkManager* m_LinkManager = nullptr;
    util::DecayingHashSet<RouterID> m_Filter;

    AbstractRouter* m_router;
  };
}  // namespace llarp
