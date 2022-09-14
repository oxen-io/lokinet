#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <optional>
#include <llarp/net/net_int.hpp>

namespace llarp
{
  struct AbstractRouter;

  struct RoutePoker : public std::enable_shared_from_this<RoutePoker>
  {
    void
    AddRoute(net::ipv4addr_t ip);

    void
    DelRoute(net::ipv4addr_t ip);

    void
    Start(AbstractRouter* router);

    ~RoutePoker();

    /// explicitly put routes up
    void
    Up();

    /// explicitly put routes down
    void
    Down();

    /// set dns resolver
    /// pass in if we are using exit node mode right now  as a bool
    void
    SetDNSMode(bool using_exit_mode) const;

   private:
    void
    Update();

    bool
    IsEnabled() const;

    void
    DeleteAllRoutes();

    void
    DisableAllRoutes();

    void
    RefreshAllRoutes();

    void
    EnableRoute(net::ipv4addr_t ip, net::ipv4addr_t gateway);

    void
    DisableRoute(net::ipv4addr_t ip, net::ipv4addr_t gateway);

    std::unordered_map<net::ipv4addr_t, net::ipv4addr_t> m_PokedRoutes;

    std::optional<net::ipv4addr_t> m_CurrentGateway;

    AbstractRouter* m_Router = nullptr;
    bool m_up{false};
  };
}  // namespace llarp
