#ifndef LLARP_GOSSIPER_HPP
#define LLARP_GOSSIPER_HPP
#include <router_contact.hpp>

namespace llarp
{
  struct I_RCGossiper
  {
    virtual ~I_RCGossiper() = default;
    /// try goissping RC
    /// return false if we hit a cooldown for this rc
    /// return true if we gossiped this rc to at least 1 peer
    virtual bool
    GossipRC(const RouterContact &rc) = 0;

    virtual void
    Decay(llarp_time_t now) = 0;
  };
}  // namespace llarp

#endif
