#pragma once

#include <llarp/util/decaying_hashset.hpp>
#include "outbound_message_handler.hpp"
#include <llarp/link/link_manager.hpp>

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
    Init(LinkManager*, const RouterID&, Router*);

    void
    Forget(const RouterID& router);

    TimePoint_t
    NextGossipAt() const;

    std::optional<TimePoint_t>
    LastGossipAt() const;

   private:
    RouterID rid;
    Time_t last_rc_gossip = 0s;
    LinkManager* link_manager = nullptr;
    util::DecayingHashSet<RouterID> filter;

    Router* router;
  };
}  // namespace llarp
