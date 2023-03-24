#include "route_poker.hpp"
#include <llarp/router/abstractrouter.hpp>
#include <llarp/net/sock_addr.hpp>
#include <llarp/dns/platform.hpp>
#include <llarp/dns/server.hpp>
#include <llarp/vpn/platform.hpp>
#include <unordered_set>

namespace llarp
{
  static auto logcat = log::Cat("route-poker");

  void
  RoutePoker::AddRoute(net::ipv4addr_t ip)
  {
    if (not m_up)
      return;
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
    // we dont need to do anything we are not enabled on runtime by config.
    if (not IsEnabled())
      return;

    auto* route_man = m_Router->get_layers()->platform->route_manager();
    auto vpn = m_Router->get_layers()->platform->vpn_interface();
    if (not(vpn and route_man))
    {
      // no vpn interface with route manager so we cannot poke routes yet.
      return;
    }

    // get current gateways, assume sorted by lowest metric first

    auto gateways = route_man->GetGatewaysNotOnInterface(*vpn);

    std::optional<net::ipv4addr_t> next_gw;
    for (auto& gateway : gateways)
    {
      if (auto* gw_ptr = std::get_if<net::ipv4addr_t>(&gateway))
      {
        next_gw = *gw_ptr;
        break;
      }
    }

    // update current gateway and apply state changes as needed
    if (m_CurrentGateway != next_gw)
    {
      if (next_gw and m_CurrentGateway)
      {
        log::info(logcat, "default gateway changed from {} to {}", *m_CurrentGateway, *next_gw);
        m_CurrentGateway = next_gw;
        m_Router->Thaw();
        RefreshAllRoutes();
      }
      else if (m_CurrentGateway)
      {
        log::warning(logcat, "default gateway {} has gone away", *m_CurrentGateway);
        m_CurrentGateway = next_gw;
        m_Router->Freeze();
      }
      else  // next_gw and not m_CurrentGateway
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
    if (m_Router->IsServiceNode())
      return;

    if (const auto& dns_server = m_Router->get_dns())
      dns_server->SetDNSMode(exit_mode_on);
  }

  void
  RoutePoker::Up()
  {
    bool was_up = m_up;
    m_up = true;
    if (not was_up)
    {
      if (not IsEnabled())
      {
        log::warning(logcat, "RoutePoker coming up, but route poking is disabled by config");
      }
      else if (not m_CurrentGateway)
      {
        log::warning(logcat, "RokerPoker came up, but we don't know of a gateway!");
      }
      else if (auto vpn = m_Router->get_layers()->platform->vpn_interface())
      {
        if (auto* route_man = m_Router->get_layers()->platform->route_manager())
        {
          log::info(logcat, "RoutePoker coming up; poking routes");
          // black hole all routes if enabled
          if (m_Router->GetConfig()->network.m_BlackholeRoutes)
            route_man->AddBlackhole();

          // explicit route pokes for first hops
          m_Router->ForEachPeer(
              [this](auto session, auto) { AddRoute(session->GetRemoteEndpoint().getIPv4()); },
              false);
          // add default route
          route_man->AddDefaultRouteViaInterface(*vpn);
        }
        log::info(logcat, "route poker up");
      }
    }
    if (not was_up)
      SetDNSMode(true);
  }

  void
  RoutePoker::Down()
  {
    // unpoke routes for first hops
    m_Router->ForEachPeer(
        [this](auto session, auto) { DelRoute(session->GetRemoteEndpoint().getIPv4()); }, false);

    // remove default route
    if (const auto& vpn = m_Router->get_layers()->platform->vpn_interface();
        vpn and IsEnabled() and m_up)
    {
      if (auto route_man = m_Router->get_layers()->platform->route_manager())
      {
        route_man->DelBlackhole();
        route_man->DelDefaultRouteViaInterface(*vpn);
      }
      log::info(logcat, "route poker down");
    }
    if (m_up)
      SetDNSMode(false);
    m_up = false;
  }

}  // namespace llarp
