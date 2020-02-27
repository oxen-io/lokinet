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
    RouterEvent(llarp::RouterID);

    virtual ~RouterEvent() = default;

    virtual void Process(RouterHive& hive) const = 0;

    virtual std::string ToString() const = 0;

    llarp::RouterID routerID;
  };

  typedef std::unique_ptr<RouterEvent> RouterEventPtr;


  struct PathBuildAttemptEvent : public RouterEvent
  {
    PathBuildAttemptEvent(const llarp::RouterID& routerID, std::vector<llarp::path::PathHopConfig> hops);

    void Process(RouterHive& hive) const;

    std::string ToString() const override;

    std::vector<llarp::path::PathHopConfig> hops;
  };

} // namespace tooling
