#include <tooling/router_event.hpp>

#include <path/path.hpp>
#include <path/transit_hop.hpp>

namespace tooling
{

  RouterEvent::RouterEvent(llarp::RouterID routerID, bool triggered)
    : routerID(routerID), triggered(triggered)
  {
  }

  PathAttemptEvent::PathAttemptEvent(const llarp::RouterID& routerID, std::shared_ptr<const llarp::path::Path> path)
    : RouterEvent(routerID, false), hops(path->hops)
  {
  }

  std::string
  PathAttemptEvent::ToString() const
  {
    std::string result = "PathAttemptEvent [";
    result += routerID.ShortString();
    result += "] ---- [";

    size_t i = 0;
    for (const auto& hop : hops)
    {
      i++;

      result += llarp::RouterID(hop.rc.pubkey).ShortString();
      result += "]";

      if (i != hops.size())
      {
        result += " -> [";
      }
    }

    return result;
  }


  PathRequestReceivedEvent::PathRequestReceivedEvent(const llarp::RouterID& routerID, std::shared_ptr<const llarp::path::TransitHop> hop)
    : RouterEvent(routerID, true)
    , prevHop(hop->info.downstream)
    , nextHop(hop->info.upstream)
  {
    isEndpoint = false;
    if (routerID == nextHop)
    {
      isEndpoint = true;
    }
  }

  std::string
  PathRequestReceivedEvent::ToString() const
  {
    std::string result = "PathRequestReceivedEvent [";
    result += routerID.ShortString();
    result += "] ---- [";
    result += prevHop.ShortString();
    result += "] -> [*";
    result += routerID.ShortString();
    result += "] -> [";

    if (isEndpoint)
    {
      result += "nowhere]";
    }
    else
    {
      result += nextHop.ShortString();
      result += "]";
    }

    return result;
  }

} // namespace tooling
