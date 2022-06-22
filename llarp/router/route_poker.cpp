#include "route_poker.hpp"
#include "abstractrouter.hpp"
#include "net/sock_addr.hpp"
#include <llarp/service/context.hpp>
#include <llarp/dns/platform.hpp>
#include <unordered_set>

namespace llarp
{
  void
  RoutePoker::AddRoute(huint32_t ip)
  {
    m_PokedRoutes[ip] = m_CurrentGateway;
    if (m_CurrentGateway.h == 0)
    {
      llarp::LogDebug("RoutePoker::AddRoute no current gateway, cannot enable route.");
    }
    else if (m_Enabled or m_Enabling)
    {
      llarp::LogInfo(
          "RoutePoker::AddRoute enabled, enabling route to ", ip, " via ", m_CurrentGateway);
      EnableRoute(ip, m_CurrentGateway);
    }
    else
    {
      llarp::LogDebug("RoutePoker::AddRoute disabled, not enabling route.");
    }
  }

  void
  RoutePoker::DisableRoute(huint32_t ip, huint32_t gateway)
  {
    vpn::IRouteManager& route = m_Router->GetVPNPlatform()->RouteManager();
    route.DelRoute(ip, gateway);
  }

  void
  RoutePoker::EnableRoute(huint32_t ip, huint32_t gateway)
  {
    vpn::IRouteManager& route = m_Router->GetVPNPlatform()->RouteManager();
    route.AddRoute(ip, gateway);
  }

  void
  RoutePoker::DelRoute(huint32_t ip)
  {
    const auto itr = m_PokedRoutes.find(ip);
    if (itr == m_PokedRoutes.end())
      return;

    if (m_Enabled)
      DisableRoute(itr->first, itr->second);
    m_PokedRoutes.erase(itr);
  }

  void
  RoutePoker::Init(AbstractRouter* router, bool enable)
  {
    m_Router = router;
    m_Enabled = enable;
    m_CurrentGateway = {0};
  }

  void
  RoutePoker::DeleteAllRoutes()
  {
    // DelRoute will check enabled, so no need here
    for (const auto& [ip, gateway] : m_PokedRoutes)
      DelRoute(ip);
  }

  void
  RoutePoker::DisableAllRoutes()
  {
    for (const auto& [ip, gateway] : m_PokedRoutes)
      DisableRoute(ip, gateway);
  }

  void
  RoutePoker::EnableAllRoutes()
  {
    for (auto& [ip, gateway] : m_PokedRoutes)
    {
      gateway = m_CurrentGateway;
      EnableRoute(ip, m_CurrentGateway);
    }
  }

  RoutePoker::~RoutePoker()
  {
    if (not m_Router or not m_Router->GetVPNPlatform())
      return;

    vpn::IRouteManager& route = m_Router->GetVPNPlatform()->RouteManager();
    for (const auto& [ip, gateway] : m_PokedRoutes)
    {
      if (gateway.h)
        route.DelRoute(ip, gateway);
    }
    route.DelBlackhole();
  }

  std::optional<huint32_t>
  RoutePoker::GetDefaultGateway() const
  {
    if (not m_Router)
      throw std::runtime_error("Attempting to use RoutePoker before calling Init");

    const auto ep = m_Router->hiddenServiceContext().GetDefault();
    vpn::IRouteManager& route = m_Router->GetVPNPlatform()->RouteManager();
    const auto gateways = route.GetGatewaysNotOnInterface(ep->GetIfName());
    if (gateways.empty())
    {
      return std::nullopt;
    }
    if (auto* ptr = std::get_if<huint32_t>(&gateways[0]))
    {
      return huint32_t{*ptr};
    }
    return std::nullopt;
  }

  void
  RoutePoker::Update()
  {
    if (not m_Router)
      throw std::runtime_error("Attempting to use RoutePoker before calling Init");

    // check for network
    const auto maybe = GetDefaultGateway();
    if (not maybe.has_value())
    {
#ifndef ANDROID
      LogError("Network is down");
#endif
      // mark network lost
      m_HasNetwork = false;
      return;
    }
    const huint32_t gateway = *maybe;

    const bool gatewayChanged = m_CurrentGateway.h != 0 and m_CurrentGateway != gateway;

    if (m_CurrentGateway != gateway)
    {
      LogInfo("found default gateway: ", gateway);
      m_CurrentGateway = gateway;
      if (m_Enabling)
      {
        EnableAllRoutes();
        Up();
      }
    }
    // revive network connectitivity on gateway change or network wakeup
    if (gatewayChanged or not m_HasNetwork)
    {
      LogInfo("our network changed, thawing router state");
      m_Router->Thaw();
      m_HasNetwork = true;
    }
  }

  void
  RoutePoker::SetDNSMode(bool exit_mode_on) const
  {
    if (auto dns_server = m_Router->hiddenServiceContext().GetDefault()->DNS())
      dns_server->SetDNSMode(exit_mode_on);
  }

  void
  RoutePoker::Enable()
  {
    if (m_Enabled)
      return;

    if (m_Router->GetConfig()->network.m_EnableRoutePoker)
    {
      m_Enabling = true;
      Update();
      m_Enabling = false;
      m_Enabled = true;
    }

    SetDNSMode(true);
  }

  void
  RoutePoker::Disable()
  {
    if (not m_Enabled)
      return;

    DisableAllRoutes();
    m_Enabled = false;

    SetDNSMode(false);
  }

  void
  RoutePoker::Up()
  {
    if (not m_Router->GetConfig()->network.m_EnableRoutePoker)
      return;

    vpn::IRouteManager& route = m_Router->GetVPNPlatform()->RouteManager();

    // black hole all routes if enabled
    if (m_Router->GetConfig()->network.m_BlackholeRoutes)
      route.AddBlackhole();

    // explicit route pokes for first hops
    m_Router->ForEachPeer(
        [&](auto session, auto) mutable { AddRoute(session->GetRemoteEndpoint().asIPv4()); },
        false);
    // add default route
    const auto ep = m_Router->hiddenServiceContext().GetDefault();
    route.AddDefaultRouteViaInterface(ep->GetIfName());
  }

  void
  RoutePoker::Down()
  {
    if (not m_Router->GetConfig()->network.m_EnableRoutePoker)
      return;

    // unpoke routes for first hops
    m_Router->ForEachPeer(
        [&](auto session, auto) mutable { DelRoute(session->GetRemoteEndpoint().asIPv4()); },
        false);
    // remove default route
    const auto ep = m_Router->hiddenServiceContext().GetDefault();
    vpn::IRouteManager& route = m_Router->GetVPNPlatform()->RouteManager();

    route.DelDefaultRouteViaInterface(ep->GetIfName());
    // delete route blackhole
    route.DelBlackhole();
  }

}  // namespace llarp
