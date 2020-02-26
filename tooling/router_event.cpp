#include "tooling/router_event.cpp"
#include "llarp/router_id.hpp"

namespace tooling
{

  PathBuildAttemptEvent::PathBuildAttemptEvent(const llarp::RouterID& routerID, std::vector<llarp::RouterID> hops)
    : routerID(routerID), hops(hops)
  {
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

      result += hop.ToString().substr(0, 8);
      result += "]";

      if (i != hops.size())
      {
        result += " -> [";
      }
    }

    return result;
  }

} // namespace tooling
