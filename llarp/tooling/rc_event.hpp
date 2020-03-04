#include <tooling/router_event.hpp>

#include <router_contact.hpp>

namespace tooling
{
  struct RCGossipReceivedEvent : public RouterEvent
  {
    RCGossipReceivedEvent(const llarp::RouterID& routerID, const llarp::RouterContact& rc);

    std::string
    ToString() const override;

    std::string
    LongString() const;

    llarp::RouterContact rc;
  };

  struct RCGossipSentEvent : public RouterEvent
  {
    RCGossipSentEvent(const llarp::RouterID& routerID, const llarp::RouterContact& rc);

    std::string
    ToString() const override;

    std::string
    LongString() const;

    llarp::RouterContact rc;
  };

} // namespace tooling

