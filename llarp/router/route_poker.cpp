#include <router/route_poker.hpp>
#include <router/abstractrouter.hpp>
#include <net/route.hpp>
#include <service/context.hpp>
#include <unordered_set>

namespace llarp
{
  void
  RoutePoker::AddRoute(huint32_t ip)
  {
    if (m_CurrentGateway.h == 0)
      return;
    m_PokedRoutes.emplace(ip, m_CurrentGateway);
    net::AddRoute(ip.ToString(), m_CurrentGateway.ToString());
  }

  void
  RoutePoker::DelRoute(huint32_t ip)
  {
    const auto itr = m_PokedRoutes.find(ip);
    if (itr == m_PokedRoutes.end())
      return;
    net::DelRoute(itr->first.ToString(), itr->second.ToString());
    m_PokedRoutes.erase(itr);
  }

  RoutePoker::~RoutePoker()
  {
    for (const auto& [ip, gateway] : m_PokedRoutes)
      net::DelRoute(ip.ToString(), gateway.ToString());
  }

  std::optional<huint32_t>
  RoutePoker::GetDefaultGateway(const AbstractRouter& router) const
  {
    const auto ep = router.hiddenServiceContext().GetDefault();
    const auto gateways = net::GetGatewaysNotOnInterface(ep->GetIfName());
    huint32_t addr{};
    if (not gateways.empty())
      addr.FromString(gateways[0]);
    return addr;
  }

  void
  RoutePoker::Update(const AbstractRouter& router)
  {
    const auto maybe = GetDefaultGateway(router);
    if (not maybe.has_value())
    {
      LogError("Network is down");
      return;
    }
    const huint32_t gateway = *maybe;
    if (m_CurrentGateway != gateway)
    {
      LogInfo("found default gateway: ", gateway);
      // unpoke current routes
      std::unordered_set<huint32_t> holes;

      for (const auto& [ip, gw] : m_PokedRoutes)
      {
        // save hole
        holes.emplace(ip);
        // unpoke route
        net::DelRoute(ip.ToString(), gw.ToString());
      }
      m_PokedRoutes.clear();

      m_CurrentGateway = gateway;
      for (const auto& ip : holes)
      {
        AddRoute(ip);
      }
    }
  }
}  // namespace llarp
