#pragma once

#include "router_event.hpp"

#include <llarp/router_contact.hpp>

namespace tooling
{
  struct RCGossipReceivedEvent : public RouterEvent
  {
    RCGossipReceivedEvent(const llarp::RouterID& routerID, const llarp::RouterContact& rc_)
        : RouterEvent("RCGossipReceivedEvent", routerID, true), rc(rc_)
    {}

    std::string
    ToString() const override
    {
      return RouterEvent::ToString()
          + " ---- other RouterID: " + llarp::RouterID(rc.pubkey).ShortString();
    }

    std::string
    LongString() const
    {
      return RouterEvent::ToString() + " ---- RC: " + rc.ToString();
    }

    llarp::RouterContact rc;
  };

  struct RCGossipSentEvent : public RouterEvent
  {
    RCGossipSentEvent(const llarp::RouterID& routerID, const llarp::RouterContact& rc_)
        : RouterEvent("RCGossipSentEvent", routerID, true), rc(rc_)
    {}

    std::string
    ToString() const override
    {
      return RouterEvent::ToString()
          + " ---- sending RC for RouterID: " + llarp::RouterID(rc.pubkey).ShortString();
    }

    std::string
    LongString() const
    {
      return RouterEvent::ToString() + " ---- RC: " + rc.ToString();
    }

    llarp::RouterContact rc;
  };

}  // namespace tooling
