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

} // namespace tooling
