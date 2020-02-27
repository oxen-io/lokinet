#pragma once

#include <string>
#include <vector>

namespace llarp
{

  struct RouterID;

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
    virtual ~RouterEvent() = default;

    virtual void Process(RouterHive& hive) const = 0;

    virtual std::string ToString() const = 0;

    llarp::RouterID routerID;
  };


  struct PathBuildAttemptEvent : public RouterEvent
  {
    PathBuildAttemptEvent(const llarp::RouterID& routerID, std::vector<llarp::path::PathHopConfig> hops);

    void Process(RouterHive& hive) const;

    std::string ToString() const override;

    std::vector<llarp::path::PathHopConfig> hops;
  };

} // namespace tooling
