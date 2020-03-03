#pragma once

#include <router_id.hpp>
#include <path/path_types.hpp>

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

  } // namespace llarp::path

} // namespace llarp


namespace tooling
{

  struct RouterHive;

  struct RouterEvent
  {
    RouterEvent(std::string eventType, llarp::RouterID routerID, bool triggered);

    virtual ~RouterEvent() = default;

    virtual std::string ToString() const;

    const std::string eventType;

    llarp::RouterID routerID;

    bool triggered = false;
  };

  using RouterEventPtr = std::unique_ptr<RouterEvent>;


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
