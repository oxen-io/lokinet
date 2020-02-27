#include <tooling/router_event.hpp>

#include <tooling/router_hive.hpp>

#include <path/path.hpp>

namespace tooling
{

  RouterEvent::RouterEvent(llarp::RouterID routerID)
    : routerID(routerID)
  {
  }

  PathBuildAttemptEvent::PathBuildAttemptEvent(const llarp::RouterID& routerID, std::vector<llarp::path::PathHopConfig> hops)
    : RouterEvent(routerID), hops(hops)
  {
  }

  void
  PathBuildAttemptEvent::Process(RouterHive& hive) const
  {
    hive.ProcessPathBuildAttempt(*this);
  }

  std::string
  PathBuildAttemptEvent::ToString() const
  {
    std::string result = "PathBuildAttemptEvent [";
    result += routerID.ToString().substr(0, 8);
    result += "] ---- [";

    size_t i = 0;
    for (const auto& hop : hops)
    {
      i++;

      result += hop.rc.pubkey.ToString().substr(0, 8);
      result += "]";

      if (i != hops.size())
      {
        result += " -> [";
      }
    }

    return result;
  }

} // namespace tooling
