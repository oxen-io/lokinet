#include "route_poker.hpp"
#include "abstractrouter.hpp"
#include "net/sock_addr.hpp"
#include <llarp/service/context.hpp>
#include <llarp/dns/platform.hpp>
#include <unordered_set>

namespace llarp
{
  static auto logcat = log::Cat("route-poker");

  void
  RoutePoker::AddRoute(net::ipv4addr_t ip)
  {
    bool has_existing = m_PokedRoutes.count(ip);
    // set up route and apply as needed
    auto& gw = m_PokedRoutes[ip];
    if (m_CurrentGateway)
    {
      // remove existing mapping as needed
      if (has_existing)
        DisableRoute(ip, gw);
      // update and add new mapping
      gw = *m_CurrentGateway;
      log::info(logcat, "add route {} via {}", ip, gw);
      EnableRoute(ip, gw);
    }
    else
      gw = net::ipv4addr_t{};
  }

  void
  RoutePoker::DisableRoute(net::ipv4addr_t ip, net::ipv4addr_t gateway)
  {
    if (ip.n and gateway.n and IsEnabled())
    {
      vpn::IRouteManager& route = m_Router->GetVPNPlatform()->RouteManager();
      route.DelRoute(ip, gateway);
    }
  }

  void
  RoutePoker::EnableRoute(net::ipv4addr_t ip, net::ipv4addr_t gateway)
  {
    if (ip.n and gateway.n and IsEnabled())
    {
      vpn::IRouteManager& route = m_Router->GetVPNPlatform()->RouteManager();
      route.AddRoute(ip, gateway);
    }
  }

  void
  RoutePoker::DelRoute(net::ipv4addr_t ip)
  {
    const auto itr = m_PokedRoutes.find(ip);
    if (itr == m_PokedRoutes.end())
      return;
    log::info(logcat, "del route {} via {}", itr->first, itr->second);
    DisableRoute(itr->first, itr->second);
    m_PokedRoutes.erase(itr);
  }

  void
  RoutePoker::Start(AbstractRouter* router)
  {
    m_Router = router;
    if (not IsEnabled())
      return;

    m_Router->loop()->call_every(100ms, weak_from_this(), [self = weak_from_this()]() {
      if (auto ptr = self.lock())
        ptr->Update();
    });
  }

  void
  RoutePoker::DeleteAllRoutes()
  {
    // DelRoute will check enabled, so no need here
    for (const auto& item : m_PokedRoutes)
      DelRoute(item.first);
  }

  void
  RoutePoker::DisableAllRoutes()
  {
    for (const auto& [ip, gateway] : m_PokedRoutes)
    {
      DisableRoute(ip, gateway);
    }
  }

  void
  RoutePoker::RefreshAllRoutes()
  {
    for (const auto& item : m_PokedRoutes)
      AddRoute(item.first);
  }

  RoutePoker::~RoutePoker()
  {
    if (not m_Router or not m_Router->GetVPNPlatform())
      return;

    auto& route = m_Router->GetVPNPlatform()->RouteManager();
    for (const auto& [ip, gateway] : m_PokedRoutes)
    {
      if (gateway.n and ip.n)
        route.DelRoute(ip, gateway);
    }
    route.DelBlackhole();
  }

  bool
  RoutePoker::IsEnabled() const
  {
    if (not m_Router)
      throw std::runtime_error{"Attempting to use RoutePoker before calling Init"};
    if (m_Router->IsServiceNode())
      return false;
    if (const auto& conf = m_Router->GetConfig())
      return conf->network.m_EnableRoutePoker;

    throw std::runtime_error{"Attempting to use RoutePoker with router with no config set"};
  }

  void
  RoutePoker::Update()
  {
    if (not m_Router)
      throw std::runtime_error{"Attempting to use RoutePoker before calling Init"};

    // ensure we have an endpoint
    auto ep = m_Router->hiddenServiceContext().GetDefault();
    if (ep == nullptr)
      return;
    // ensure we have a vpn platform
    auto* platform = m_Router->GetVPNPlatform();
    if (platform == nullptr)
      return;
    // ensure we have a vpn interface
    auto* vpn = ep->GetVPNInterface();
    if (vpn == nullptr)
      return;

    auto& route = platform->RouteManager();

    // find current gateways
    auto gateways = route.GetGatewaysNotOnInterface(*vpn);
    std::optional<net::ipv4addr_t> next_gw;
    for (auto& gateway : gateways)
    {
      if (auto* gw_ptr = std::get_if<net::ipv4addr_t>(&gateway))
        next_gw = *gw_ptr;
    }

    // update current gateway and apply state chnages as needed
    if (not(m_CurrentGateway == next_gw))
    {
      if (next_gw and m_CurrentGateway)
      {
        log::info(logcat, "default gateway changed from {} to {}", *m_CurrentGateway, *next_gw);
        m_CurrentGateway = next_gw;
        m_Router->Thaw();
        if (m_Router->HasClientExit())
          Up();
        else
          RefreshAllRoutes();
      }
      else if (m_CurrentGateway)
      {
        log::warning(logcat, "default gateway {} has gone away", *m_CurrentGateway);
        m_CurrentGateway = next_gw;
        m_Router->Freeze();
      }
      else if (next_gw)
      {
        log::info(logcat, "default gateway found at {}", *next_gw);
        m_CurrentGateway = next_gw;
      }
    }
    else if (m_Router->HasClientExit())
      Up();
  }

  void
  RoutePoker::SetDNSMode(bool exit_mode_on) const
  {
    auto ep = m_Router->hiddenServiceContext().GetDefault();
    if (not ep)
      return;
    if (auto dns_server = ep->DNS())
      dns_server->SetDNSMode(exit_mode_on);
  }

  void
  RoutePoker::Up()
  {
    if (IsEnabled() and m_CurrentGateway and not m_up)
    {
      vpn::IRouteManager& route = m_Router->GetVPNPlatform()->RouteManager();

      // black hole all routes if enabled
      if (m_Router->GetConfig()->network.m_BlackholeRoutes)
        route.AddBlackhole();

      // explicit route pokes for first hops
      m_Router->ForEachPeer(
          [this](auto session, auto) { AddRoute(session->GetRemoteEndpoint().getIPv4()); }, false);
      // add default route
      const auto ep = m_Router->hiddenServiceContext().GetDefault();
      if (auto* vpn = ep->GetVPNInterface())
        route.AddDefaultRouteViaInterface(*vpn);
      log::info(logcat, "route poker up");
    }
    if (not m_up)
      SetDNSMode(true);
    m_up = true;
  }

  void
  RoutePoker::Down()
  {
    // unpoke routes for first hops
    m_Router->ForEachPeer(
        [this](auto session, auto) { DelRoute(session->GetRemoteEndpoint().getIPv4()); }, false);

    // remove default route

    if (IsEnabled() and m_up)
    {
      vpn::IRouteManager& route = m_Router->GetVPNPlatform()->RouteManager();
      const auto ep = m_Router->hiddenServiceContext().GetDefault();
      if (auto* vpn = ep->GetVPNInterface())
        route.DelDefaultRouteViaInterface(*vpn);

      // delete route blackhole
      route.DelBlackhole();
      log::info(logcat, "route poker down");
    }
    if (m_up)
      SetDNSMode(false);
    m_up = false;
  }

}  // namespace llarp
