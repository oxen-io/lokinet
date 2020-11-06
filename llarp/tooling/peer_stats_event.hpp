#pragma once

#include "router_event.hpp"

namespace tooling
{
  struct LinkSessionEstablishedEvent : public RouterEvent
  {
    llarp::RouterID remoteId;
    bool inbound = false;

    LinkSessionEstablishedEvent(
        const llarp::RouterID& ourRouterId, const llarp::RouterID& remoteId_, bool inbound_)
        : RouterEvent("Link: LinkSessionEstablishedEvent", ourRouterId, false)
        , remoteId(remoteId_)
        , inbound(inbound_)
    {}

    std::string
    ToString() const
    {
      return RouterEvent::ToString() + (inbound ? "inbound" : "outbound")
          + " : LinkSessionEstablished with " + remoteId.ToString();
    }
  };

  struct ConnectionAttemptEvent : public RouterEvent
  {
    llarp::RouterID remoteId;

    ConnectionAttemptEvent(const llarp::RouterID& ourRouterId, const llarp::RouterID& remoteId_)
        : RouterEvent("Link: ConnectionAttemptEvent", ourRouterId, false), remoteId(remoteId_)
    {}

    std::string
    ToString() const
    {
      return RouterEvent::ToString() + " : LinkSessionEstablished with " + remoteId.ToString();
    }
  };

}  // namespace tooling
