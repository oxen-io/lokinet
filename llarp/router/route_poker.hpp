#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <optional>
#include <llarp/net/net_int.hpp>
#include "systemd_resolved.hpp"

namespace llarp
{
  struct AbstractRouter;

  struct RoutePoker
  {
    void
    AddRoute(SockAddr addr);

    void
    DelRoute(SockAddr addr);

    void
    Init(AbstractRouter* router, bool enable = false);

    ~RoutePoker();

    void
    Update();

    // sets stored routes and causes AddRoute to actually
    // set routes rather than just storing them
    void
    Enable();

    // unsets stored routes, and causes AddRoute to simply
    // remember the desired routes rather than setting them.
    void
    Disable();

    /// explicitly put routes up
    void
    Up();

    /// explicitly put routes down
    void
    Down();

   private:
    void
    DeleteAllRoutes();

    void
    DisableAllRoutes();

    void
    EnableAllRoutes();

    void
    EnableRoute(huint32_t ip, huint32_t gateway, huint16_t port);

    void
    DisableRoute(huint32_t ip, huint32_t gateway, huint16_t port);

    std::optional<huint32_t>
    GetDefaultGateway() const;

    std::unordered_map<huint32_t, huint32_t> m_PokedRoutes;
    std::unordered_multimap<huint32_t, huint16_t> m_IPToPort;
    huint32_t m_CurrentGateway;

    bool m_Enabled = false;
    bool m_Enabling = false;

    AbstractRouter* m_Router = nullptr;

    bool m_HasNetwork = true;
  };
}  // namespace llarp
