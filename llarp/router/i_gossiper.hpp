#pragma once
#include <llarp/router_contact.hpp>

namespace llarp
{
  /// The maximum number of peers we will flood a gossiped RC to when propagating an RC
  constexpr size_t MaxGossipPeers = 20;

  struct I_RCGossiper
  {
    virtual ~I_RCGossiper() = default;
    /// try goissping RC
    /// return false if we hit a cooldown for this rc
    /// return true if we gossiped this rc to at least 1 peer
    virtual bool
    GossipRC(const RouterContact& rc) = 0;

    using Time_t = std::chrono::milliseconds;

    virtual void
    Decay(Time_t now) = 0;

    /// return true if we should gossip our RC now
    virtual bool
    ShouldGossipOurRC(Time_t now) const = 0;

    /// return true if that rc is owned by us
    virtual bool
    IsOurRC(const RouterContact& rc) const = 0;

    /// forget the replay filter entry given pubkey
    virtual void
    Forget(const RouterID& router) = 0;
  };
}  // namespace llarp
