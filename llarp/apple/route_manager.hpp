#pragma once

#include <llarp/router/abstractrouter.hpp>
#include <llarp/vpn/platform.hpp>
#include "context_wrapper.h"

namespace llarp::apple
{
  class RouteManager final : public llarp::vpn::IRouteManager
  {
   public:
    RouteManager(llarp::Context& ctx, llarp_route_callbacks rcs, void* callback_context)
        : context{ctx}, callback_context{callback_context}, route_callbacks{std::move(rcs)}
    {}

    /// These are called for poking route holes, but we don't have to do that at all on macos
    /// because the appex isn't subject to its own rules.
    void
    AddRoute(net::ipaddr_t /*ip*/, net::ipaddr_t /*gateway*/) override
    {}

    void
    DelRoute(net::ipaddr_t /*ip*/, net::ipaddr_t /*gateway*/) override
    {}

    void
    AddDefaultRouteViaInterface(vpn::NetworkInterface& vpn) override;

    void
    DelDefaultRouteViaInterface(vpn::NetworkInterface& vpn) override;

    void
    AddRouteViaInterface(vpn::NetworkInterface& vpn, IPRange range) override;

    void
    DelRouteViaInterface(vpn::NetworkInterface& vpn, IPRange range) override;

    std::vector<net::ipaddr_t>
    GetGatewaysNotOnInterface(vpn::NetworkInterface& /*vpn*/) override
    {
      // We can't get this on mac from our sandbox, but we don't actually need it because we
      // ignore the gateway for AddRoute/DelRoute anyway, so just return a zero IP.
      std::vector<net::ipaddr_t> ret;
      ret.emplace_back(net::ipv4addr_t{});
      return ret;
    }

   private:
    llarp::Context& context;
    bool trampoline_active = false;
    std::vector<llarp::SockAddr> saved_upstream_dns;
    void
    check_trampoline(bool enable);

    void* callback_context = nullptr;
    llarp_route_callbacks route_callbacks;
  };

}  // namespace llarp::apple
