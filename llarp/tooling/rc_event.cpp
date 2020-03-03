#include <tooling/rc_event.hpp>

namespace tooling
{
  RCGossipReceivedEvent::RCGossipReceivedEvent(const llarp::RouterID& routerID, const llarp::RouterContact& rc)
    : RouterEvent("RCGossipReceivedEvent", routerID, true)
    , rc(rc)
  {
  }

  std::string
  RCGossipReceivedEvent::ToString() const
  {
    return RouterEvent::ToString() +  " ---- other RouterID: " + llarp::RouterID(rc.pubkey).ShortString();
  }

  std::string
  RCGossipReceivedEvent::LongString() const
  {
    return RouterEvent::ToString() + " ---- RC: " + rc.ToString();
  }

} // namespace tooling

