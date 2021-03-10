#include "router_event.hpp"

#include <llarp/path/path_types.hpp>
#include <llarp/path/path.hpp>
#include <llarp/path/transit_hop.hpp>

namespace tooling
{
  struct PathAttemptEvent : public RouterEvent
  {
    std::vector<llarp::path::PathHopConfig> hops;
    llarp::PathID_t pathid;

    PathAttemptEvent(const llarp::RouterID& routerID, std::shared_ptr<const llarp::path::Path> path)
        : RouterEvent("PathAttemptEvent", routerID, false)
        , hops(path->hops)
        , pathid(path->hops[0].rxID)
    {}

    std::string
    ToString() const
    {
      std::string result = RouterEvent::ToString();
      result += "---- [";

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
  };

  struct PathRequestReceivedEvent : public RouterEvent
  {
    llarp::RouterID prevHop;
    llarp::RouterID nextHop;
    llarp::PathID_t txid;
    llarp::PathID_t rxid;
    bool isEndpoint = false;

    PathRequestReceivedEvent(
        const llarp::RouterID& routerID, std::shared_ptr<const llarp::path::TransitHop> hop)
        : RouterEvent("PathRequestReceivedEvent", routerID, true)
        , prevHop(hop->info.downstream)
        , nextHop(hop->info.upstream)
        , txid(hop->info.txID)
        , rxid(hop->info.rxID)
        , isEndpoint(routerID == nextHop ? true : false)
    {}

    std::string
    ToString() const
    {
      std::string result = RouterEvent::ToString();
      result += "---- [";
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
  };

  struct PathStatusReceivedEvent : public RouterEvent
  {
    llarp::PathID_t rxid;
    uint64_t status;

    PathStatusReceivedEvent(
        const llarp::RouterID& routerID, const llarp::PathID_t rxid_, uint64_t status_)
        : RouterEvent("PathStatusReceivedEvent", routerID, true), rxid(rxid_), status(status_)
    {}

    std::string
    ToString() const
    {
      std::string result = RouterEvent::ToString();
      result += "---- path rxid: " + rxid.ShortHex();
      result += ", status: " + std::to_string(status);

      return result;
    }
  };

  struct PathBuildRejectedEvent : public RouterEvent
  {
    llarp::PathID_t rxid;
    llarp::RouterID rejectedBy;

    PathBuildRejectedEvent(
        const llarp::RouterID& routerID,
        const llarp::PathID_t rxid_,
        const llarp::RouterID& rejectedBy_)
        : RouterEvent("PathBuildRejectedEvent", routerID, false)
        , rxid(rxid_)
        , rejectedBy(rejectedBy_)
    {}

    std::string
    ToString() const
    {
      std::string result = RouterEvent::ToString();
      result += "---- path rxid: " + rxid.ShortHex();
      result += ", rejectedBy: " + rejectedBy.ShortString();

      return result;
    }
  };

}  // namespace tooling
