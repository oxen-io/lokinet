#pragma once

#include <string>
#include <vector>

namespace llarp
{

  struct RouterID;

} // namespace llarp


namespace tooling
{

  struct RouterEvent
  {
    virtual ~RouterEvent() = default;

    virtual std::string ToString() const = 0;

    llarp::RouterID routerID;
  };

  struct PathBuildAttemptEvent : public RouterEvent
  {
    PathBuildAttemptEvent(const llarp::RouterID& routerID, std::vector<llarp::RouterID> hops);

    std::string ToString() const override;

    std::vector<llarp::RouterID> hops;
  }

} // namespace tooling
