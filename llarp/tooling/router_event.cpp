#include <tooling/router_event.hpp>

namespace tooling
{

  RouterEvent::RouterEvent(std::string eventType, llarp::RouterID routerID, bool triggered)
    : eventType(eventType), routerID(routerID), triggered(triggered)
  {
  }

  std::string
  RouterEvent::ToString() const
  {
    std::string result;
    result += eventType;
    result += " [";
    result += routerID.ShortString();
    result += "] -- ";
    return result;
  }

} // namespace tooling
