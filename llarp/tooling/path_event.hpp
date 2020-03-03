#include "router_event.hpp"

#include <path/path_types.hpp>

namespace tooling
{

  struct PathAttemptEvent : public RouterEvent
  {
    PathAttemptEvent(const llarp::RouterID& routerID, std::shared_ptr<const llarp::path::Path> path);

    std::string ToString() const override;

    std::vector<llarp::path::PathHopConfig> hops;

    llarp::PathID_t pathid;
  };

  struct PathRequestReceivedEvent : public RouterEvent
  {
    PathRequestReceivedEvent(const llarp::RouterID& routerID, std::shared_ptr<const llarp::path::TransitHop> hop);

    std::string ToString() const override;

    llarp::RouterID prevHop;
    llarp::RouterID nextHop;

    llarp::PathID_t txid;
    llarp::PathID_t rxid;

    bool isEndpoint = false;
  };

  struct PathStatusReceivedEvent : public RouterEvent
  {
    PathStatusReceivedEvent(const llarp::RouterID& routerID, const llarp::PathID_t rxid, uint64_t status);

    std::string ToString() const override;

    llarp::PathID_t rxid;

    uint64_t status;
  };

} // namespace tooling
