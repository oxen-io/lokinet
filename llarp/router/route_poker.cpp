#include "route_poker.hpp"
#include "abstractrouter.hpp"
#include "net/sock_addr.hpp"
#include <llarp/service/context.hpp>
#include <unordered_set>

namespace llarp
{
  void
  RoutePoker::AddRoute(SockAddr addr)
  {
    const auto ip = addr.asIPv4();

    const huint16_t port{addr.getPort()};

    m_PokedRoutes[ip] = m_CurrentGateway;

    m_IPToPort.emplace(ip, port);

    if (m_CurrentGateway.h == 0)
    {
      llarp::LogDebug("RoutePoker::AddRoute no current gateway, cannot enable route.");
    }
    else if (m_Enabled or m_Enabling)
    {
      llarp::LogInfo(
          "RoutePoker::AddRoute enabled, enabling route to ",
          ip,
          ":",
          port,
          " via ",
          m_CurrentGateway);
      EnableRoute(ip, m_CurrentGateway, port);
    }
    else
    {
      llarp::LogDebug("RoutePoker::AddRoute disabled, not enabling route.");
    }
  }

  void
  RoutePoker::DisableRoute(huint32_t ip, huint32_t gateway, huint16_t port)
  {
    vpn::IRouteManager& route = m_Router->GetVPNPlatform()->RouteManager();
    route.DelRoute(ip, gateway, port);
  }

  void
  RoutePoker::EnableRoute(huint32_t ip, huint32_t gateway, huint16_t port)
  {
    vpn::IRouteManager& route = m_Router->GetVPNPlatform()->RouteManager();
    route.AddRoute(ip, gateway, port);
  }

  void
  RoutePoker::DelRoute(SockAddr addr)

  {
    const auto ip = addr.asIPv4();
    const huint16_t port{addr.getPort()};

    const auto itr = m_PokedRoutes.find(ip);
    if (itr == m_PokedRoutes.end())
      return;

    /// remove appropriate ip to port mappings
    const auto range = m_IPToPort.equal_range(ip);
    for (auto range_itr = range.first; range_itr != range.second;)
    {
      if (range_itr->second == port)
        range_itr = m_IPToPort.erase(range_itr);
      else
        ++range_itr;
    }

    if (m_Enabled)
      DisableRoute(itr->first, itr->second, port);
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
    if (m_Enabled)
      DisableAllRoutes();

    m_PokedRoutes.clear();
    m_IPToPort.clear();
  }

  void
  RoutePoker::DisableAllRoutes()
  {
    for (const auto& [ip, gateway] : m_PokedRoutes)
    {
      const auto range = m_IPToPort.equal_range(ip);
      for (auto itr = range.first; itr != range.second; ++itr)
        DisableRoute(itr->first, gateway, itr->second);
    }
  }

  void
  RoutePoker::EnableAllRoutes()
  {
    for (auto& [ip, gateway] : m_PokedRoutes)
    {
      gateway = m_CurrentGateway;
      const auto range = m_IPToPort.equal_range(ip);
      for (auto itr = range.first; itr != range.second; ++itr)
        EnableRoute(itr->first, m_CurrentGateway, itr->second);
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
      {
        const auto range = m_IPToPort.equal_range(ip);
        for (auto itr = range.first; itr != range.second; ++itr)
          route.DelRoute(itr->first, gateway, itr->second);
      }
    }
    route.DelBlackhole();
  }

  std::optional<huint32_t>
  RoutePoker::GetDefaultGateway() const
  {
    if (not m_Router)
      throw std::runtime_error("Attempting to use RoutePoker before calling Init");
    const auto ep = m_Router->hiddenServiceContext().GetDefault();
    if (auto vpn = ep->GetVPNInterface())
    {
      vpn::IRouteManager& route = m_Router->GetVPNPlatform()->RouteManager();
      const auto gateways = route.GetGatewaysNotOnInterface(*vpn);
      if (gateways.empty())
        return std::nullopt;

      if (auto* ptr = std::get_if<huint32_t>(&gateways[0]))
      {
        return huint32_t{*ptr};
      }
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

    systemd_resolved_set_dns(
        m_Router->hiddenServiceContext().GetDefault()->GetIfName(),
        m_Router->GetConfig()->dns.m_bind,
        true /* route all DNS */);
  }

  void
  RoutePoker::Disable()
  {
    if (not m_Enabled)
      return;

    DisableAllRoutes();
    m_Enabled = false;

    systemd_resolved_set_dns(
        m_Router->hiddenServiceContext().GetDefault()->GetIfName(),
        m_Router->GetConfig()->dns.m_bind,
        false /* route DNS only for .loki/.snode */);
  }

  void
  RoutePoker::Up()
  {
    const auto ep = m_Router->hiddenServiceContext().GetDefault();
    const auto& enablePoker = m_Router->GetConfig()->network.m_EnableRoutePoker;
    if (auto vpn = ep->GetVPNInterface(); vpn and enablePoker)
    {
      LogInfo("route poker putting up firewall");
      vpn::IRouteManager& route = m_Router->GetVPNPlatform()->RouteManager();
      try
      {
        // black hole all routes by default
        route.AddBlackhole();
        // explicit route pokes for first hops
        m_Router->ForEachPeer(
            [&](auto session, auto) mutable { AddRoute(session->GetRemoteEndpoint()); }, false);
        // add default route
        route.AddDefaultRouteViaInterface(*vpn);
      }
      catch (std::exception& ex)
      {
        LogError("failed to put up firewall: ", ex.what());
        Down();
      }
    }
  }

  void
  RoutePoker::Down()
  {
    if (not m_Router->GetConfig()->network.m_EnableRoutePoker)
      return;
    LogInfo("route poker removing firewall...");
    // unpoke routes for first hops
    m_Router->ForEachPeer(
        [&](auto session, auto) mutable {
          auto addr = session->GetRemoteEndpoint();
          try
          {
            DelRoute(addr);
          }
          catch (std::exception& ex)
          {
            LogError("Failed to remove route hole for ", addr, ": ", ex.what());
          }
        },
        false);
    // remove default route
    const auto ep = m_Router->hiddenServiceContext().GetDefault();
    if (auto vpn = ep->GetVPNInterface())
    {
      vpn::IRouteManager& route = m_Router->GetVPNPlatform()->RouteManager();
      try
      {
        route.DelDefaultRouteViaInterface(*vpn);
        // delete route blackhole
        route.DelBlackhole();
      }
      catch (std::exception& ex)
      {
        LogError("Failed to tear down firewall: ", ex.what());
      }
    }
  }

}  // namespace llarp
