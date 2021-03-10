#pragma once

#include <llarp/router_id.hpp>

#include <string>
#include <vector>
#include <memory>

namespace llarp
{
  struct PathID_t;

  namespace path
  {
    struct Path;
    struct PathHopConfig;

    struct TransitHop;

  }  // namespace path

}  // namespace llarp

namespace tooling
{
  struct RouterHive;

  struct RouterEvent
  {
    RouterEvent(std::string eventType, llarp::RouterID routerID, bool triggered)
        : eventType(eventType), routerID(routerID), triggered(triggered)
    {}

    virtual ~RouterEvent() = default;

    virtual std::string
    ToString() const
    {
      std::string result;
      result += eventType;
      result += " [";
      result += routerID.ShortString();
      result += "] -- ";
      return result;
    }

    const std::string eventType;

    llarp::RouterID routerID;

    bool triggered = false;
  };

  using RouterEventPtr = std::unique_ptr<RouterEvent>;

}  // namespace tooling
