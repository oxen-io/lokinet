#pragma once

#include <unordered_map>
#include <string>
#include <net/net_int.hpp>

namespace llarp
{
  struct AbstractRouter;

  struct RoutePoker
  {
    void
    AddRoute(huint32_t ip);

    void
    DelRoute(huint32_t ip);

    ~RoutePoker();

    void
    Update(const AbstractRouter& router);

   private:
    std::optional<huint32_t>
    GetDefaultGateway(const AbstractRouter& router) const;

    std::unordered_map<huint32_t, huint32_t> m_PokedRoutes;
    huint32_t m_CurrentGateway;
  };
}  // namespace llarp
