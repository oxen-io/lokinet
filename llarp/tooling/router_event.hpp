#pragma once

#include <router_id.hpp>

#include <string>
#include <vector>
#include <memory>

namespace llarp
{

  namespace path
  {
    struct PathHopConfig;

  } // namespace llarp::path

} // namespace llarp


namespace tooling
{

  struct RouterHive;

  struct RouterEvent
  {
    RouterEvent(llarp::RouterID, bool triggered);

    virtual ~RouterEvent() = default;

    virtual std::string ToString() const = 0;

    llarp::RouterID routerID;

    bool triggered = false;
  };

  using RouterEventPtr = std::unique_ptr<RouterEvent>;


  struct PathBuildAttemptEvent : public RouterEvent
  {
    PathBuildAttemptEvent(const llarp::RouterID& routerID, std::vector<llarp::path::PathHopConfig> hops);

    std::string ToString() const override;

    std::vector<llarp::path::PathHopConfig> hops;
  };

} // namespace tooling
