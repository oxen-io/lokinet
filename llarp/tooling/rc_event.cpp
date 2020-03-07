#include <tooling/rc_event.hpp>

namespace tooling
{
  RCGossipReceivedEvent::RCGossipReceivedEvent(const llarp::RouterID& routerID,
                                               const llarp::RouterContact& rc)
      : RouterEvent("RCGossipReceivedEvent", routerID, true), rc(rc)
  {
  }

  std::string
  RCGossipReceivedEvent::ToString() const
  {
    return RouterEvent::ToString()
        + " ---- other RouterID: " + llarp::RouterID(rc.pubkey).ShortString();
  }

  std::string
  RCGossipReceivedEvent::LongString() const
  {
    return RouterEvent::ToString() + " ---- RC: " + rc.ToString();
  }

  RCGossipSentEvent::RCGossipSentEvent(const llarp::RouterID& routerID,
                                       const llarp::RouterContact& rc)
      : RouterEvent("RCGossipSentEvent", routerID, true), rc(rc)
  {
  }

  std::string
  RCGossipSentEvent::ToString() const
  {
    return RouterEvent::ToString() + " ---- sending RC for RouterID: "
        + llarp::RouterID(rc.pubkey).ShortString();
  }

  std::string
  RCGossipSentEvent::LongString() const
  {
    return RouterEvent::ToString() + " ---- RC: " + rc.ToString();
  }

}  // namespace tooling
