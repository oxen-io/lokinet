#include "route_manager.hpp"

namespace llarp::apple {

void RouteManager::AddDefaultRouteViaInterface(std::string)
{
    LogWarn("AddDefaultRouteViaInterface with cbctx=", (bool) callback_context, ", adr=", (bool) route_callbacks.add_default_route);
    if (callback_context and route_callbacks.add_default_route)
        route_callbacks.add_default_route(callback_context);
}

void RouteManager::DelDefaultRouteViaInterface(std::string)
{
    LogWarn("DelDefaultRouteViaInterface with cbctx=", (bool) callback_context, ", ddr=", (bool) route_callbacks.del_default_route);
    if (callback_context and route_callbacks.del_default_route)
        route_callbacks.del_default_route(callback_context);
}

void
RouteManager::AddRouteViaInterface(vpn::NetworkInterface&, IPRange range)
{
    LogWarn("AddRoute with cbctx=", (bool) callback_context, ", a4r=", (bool) route_callbacks.add_ipv4_route,
            "a6r", (bool) route_callbacks.add_ipv6_route);

    if (callback_context)
    {
        if (range.IsV4()) {
            if (route_callbacks.add_ipv4_route)
                route_callbacks.add_ipv4_route(
                        range.BaseAddressString().c_str(),
                        net::TruncateV6(range.netmask_bits).ToString().c_str(),
                        callback_context);
        } else {
            if (route_callbacks.add_ipv6_route)
                route_callbacks.add_ipv6_route(range.BaseAddressString().c_str(), range.HostmaskBits(), callback_context);
        }
    }
}

void
RouteManager::DelRouteViaInterface(vpn::NetworkInterface&, IPRange range)
{
    LogWarn("DelRoute with cbctx=", (bool) callback_context, ", a4r=", (bool) route_callbacks.del_ipv4_route,
            "a6r", (bool) route_callbacks.del_ipv6_route);

    if (callback_context)
    {
        if (range.IsV4()) {
            if (route_callbacks.del_ipv4_route)
                route_callbacks.del_ipv4_route(
                        range.BaseAddressString().c_str(),
                        net::TruncateV6(range.netmask_bits).ToString().c_str(),
                        callback_context);
        } else {
            if (route_callbacks.del_ipv6_route)
                route_callbacks.del_ipv6_route(range.BaseAddressString().c_str(), range.HostmaskBits(), callback_context);
        }
    }
}

}
