#include "route_manager.hpp"

#include <llarp.hpp>
#include <llarp/handlers/tun.hpp>

#include <memory>

namespace llarp::apple
{
    static auto logcat = log::Cat("apple.route_manager");

    void RouteManager::check_trampoline(bool enable)
    {
        if (trampoline_active == enable)
            return;
        auto router = context.router;
        if (!router)
        {
            log::error(logcat, "Cannot reconfigure to use DNS trampoline: no router");
            return;
        }

        auto& tun = router->tun_endpoint();
        if (!tun)
        {
            log::error(logcat, "Cannot reconfigure to use DNS trampoline: no tun endpoint found (!?)");
            return;
        }

        if (enable)
            tun.reconfigure_dns({oxen::quic::Address{"127.0.0.1", dns_trampoline_port}});
        else
            tun->reconfigure_dns(router->config()->dns._upstream_dns);

        trampoline_active = enable;
    }

    void RouteManager::add_default_route_via_interface(vpn::NetworkInterface&)
    {
        check_trampoline(true);
        if (callback_context and route_callbacks.add_default_route)
            route_callbacks.add_default_route(callback_context);
    }

    void RouteManager::delete_default_route_via_interface(vpn::NetworkInterface&)
    {
        check_trampoline(false);
        if (callback_context and route_callbacks.del_default_route)
            route_callbacks.del_default_route(callback_context);
    }

    void RouteManager::add_route_via_interface(vpn::NetworkInterface&, ipv4_range range)
    {
        check_trampoline(true);
        if (callback_context)
        {
            if (route_callbacks.add_ipv4_route)
                route_callbacks.add_ipv4_route(
                    range.BaseAddressString().c_str(), std::string{range.mask}.c_str(), callback_context);
        }
    }

    void RouteManager::add_route_via_interface(vpn::NetworkInterface&, ipv6_range range)
    {
        check_trampoline(true);
        if (callback_context)
        {
            if (route_callbacks.add_ipv6_route)
                route_callbacks.add_ipv6_route(range.BaseAddressString().c_str(), range.mask, callback_context);
        }
    }

    void RouteManager::delete_route_via_interface(vpn::NetworkInterface&, ipv4_range range)
    {
        check_trampoline(false);
        if (callback_context)
        {
            if (route_callbacks.del_ipv4_route)
                route_callbacks.del_ipv4_route(
                    range.ip.to_string().c_str(), std::string{range.mask}.c_str(), callback_context);
        }
    }

    void RouteManager::delete_route_via_interface(vpn::NetworkInterface&, ipv6_range range)
    {
        check_trampoline(false);
        if (callback_context)
        {
            if (route_callbacks.del_ipv6_route)
                route_callbacks.del_ipv6_route(range.ip.to_string().c_str(), range.mask, callback_context);
        }
    }

}  // namespace llarp::apple
