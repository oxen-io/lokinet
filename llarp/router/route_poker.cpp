#include "route_poker.hpp"

#include "router.hpp"

#include <llarp/link/link_manager.hpp>

namespace llarp
{
    static auto logcat = log::Cat("route_poker");

    RoutePoker::RoutePoker(Router& r) : _router{r} {}

    template <IP46 IP>
    void RoutePoker::add_route(const IP& ip)
    {
        if (not _up)
            return;

        // set up route and apply as needed
        auto& poked_rs = poked_routes<IP>();
        auto [it, new_route] = poked_rs.emplace(ip, IP{});
        auto& gw = it->second;

        auto& current_gw = current_gateway<IP>();
        if (!current_gw)
        {
            gw = IP{};
            return;
        }

        // remove existing mapping as needed
        if (!new_route)
            disable_route(ip, gw);
        // update and add new mapping
        gw = *current_gw;

        log::info(logcat, "Added route to {} via {}", ip, gw);

        enable_route(ip, gw);
    }
    template void RoutePoker::add_route(const ipv4& ip);
    template void RoutePoker::add_route(const ipv6& ip);

    template <IP46 IP>
    void RoutePoker::disable_route(const IP& ip, const IP& gateway)
    {
        if (_enabled and ip != IP{})
        {
            log::info(logcat, "Deleting route to {} via {}", ip, gateway);
            _router.vpn_platform()->RouteManager().delete_route(ip, gateway);
        }
    }
    template void RoutePoker::disable_route(const ipv4&, const ipv4&);
    template void RoutePoker::disable_route(const ipv6&, const ipv6&);

    template <IP46 IP>
    void RoutePoker::enable_route(const IP& ip, const IP& gateway)
    {
        if (_enabled and ip != IP{})
            _router.vpn_platform()->RouteManager().add_route(ip, gateway);
    }
    template void RoutePoker::enable_route(const ipv4&, const ipv4&);
    template void RoutePoker::enable_route(const ipv6&, const ipv6&);

    void RoutePoker::delete_all_routes()
    {
        for (auto it = poked_routes4.begin(); it != poked_routes4.end();)
            it = delete_route(it);
        for (auto it = poked_routes6.begin(); it != poked_routes6.end();)
            it = delete_route(it);
    }

    void RoutePoker::start()
    {
        if (not _enabled)
        {
            log::info(logcat, "Route poker is NOT enabled for this lokinet instance!");
            return;
        }

        // TODO FIXME: this should be enabled again, but it doesn't seem very nice to have this on
        // such a tight timer.  Perhaps we could trigger it from the appropriate place, and if not,
        // at least reduce the timer frequency.

        // router.loop()->call_every(100ms, weak_from_this(), [self = weak_from_this()]() {
        //     if (auto ptr = self.lock())
        //         ptr->update();
        // });
    }

    void RoutePoker::disable_all_routes()
    {
        for (const auto& [ip, gateway] : poked_routes4)
            disable_route(ip, gateway);
        for (const auto& [ip, gateway] : poked_routes6)
            disable_route(ip, gateway);
    }

    void RoutePoker::refresh_all_routes()
    {
        for (const auto& [ip, gw] : poked_routes4)
            add_route(ip);
        for (const auto& [ip, gw] : poked_routes6)
            add_route(ip);
    }

    RoutePoker::~RoutePoker()
    {
        if (not _router.vpn_platform())
            return;

        delete_all_routes();
        _router.vpn_platform()->RouteManager().delete_blackhole();
    }

    void RoutePoker::update()
    {
        // TODO FIXME
        //
        // // ensure we have an endpoint
        // auto ep = router.hidden_service_context().GetDefault();
        // if (ep == nullptr)
        //   return;
        // // ensure we have a vpn platform
        // auto* platform = router.vpn_platform();
        // if (platform == nullptr)
        //   return;
        // // ensure we have a vpn interface
        // auto* vpn = ep->GetVPNInterface();
        // if (vpn == nullptr)
        //   return;

        // auto& route = platform->RouteManager();

        // // get current gateways, assume sorted by lowest metric first
        // auto gateways = route.get_non_interface_gateways(*vpn);
        // std::optional<quic::Address> next_gw;

        // for (auto& g : gateways)
        // {
        //   if (g.is_ipv4())
        //   {
        //     next_gw = g;
        //     break;
        //   }
        // }

        // // update current gateway and apply state changes as needed
        // if (!(current_gateway == next_gw))
        // {
        //   if (next_gw and current_gateway)
        //   {
        //     log::info(logcat, "default gateway changed from {} to {}", *current_gateway,
        //     *next_gw); current_gateway = next_gw; refresh_all_routes();
        //   }
        //   else if (current_gateway)
        //   {
        //     log::warning(logcat, "default gateway {} has gone away", *current_gateway);
        //     current_gateway = next_gw;
        //   }
        //   else  // next_gw and not m_CurrentGateway
        //   {
        //     log::info(logcat, "default gateway found at {}", *next_gw);
        //     current_gateway = next_gw;
        //   }
        // }
        // else if (router.HasClientExit())
        //   put_up();
    }

    template <IP46 IP>
    inline static constexpr auto ip_name = std::same_as<IP, ipv4> ? "IPv4"sv : "IPv6"sv;

    void RoutePoker::put_up()
    {
        if (_up)
            return;
        _up = true;

        if (!_enabled)
        {
            log::warning(logcat, "RoutePoker coming up, but route poking is disabled by config");
            return;
        }

        if (!current_gateway4 && !current_gateway6)
        {
            log::warning(logcat, "RoutePoker came up, but we don't appear to have any gateways!");
            return;
        }

        log::info(logcat, "RoutePoker coming up; poking routes");

        vpn::AbstractRouteManager& route = _router.vpn_platform()->RouteManager();

        // black hole all routes if enabled
        if (_router.config().network.blackhole_routes)
            route.add_blackhole();

        // explicit route pokes for first hops
        _router.for_each_connection([this](const RouterID&, link::Connection& conn) {
            auto remote = conn.conn->remote();
            if (remote.is_ipv4())
                add_route(remote.to_ipv4());
            else
                add_route(remote.to_ipv6());
        });

        auto& local = _router.link_manager().local();
        if (local.is_ipv4())
            add_route(local.to_ipv4());
        else
            add_route(local.to_ipv6());
        // add default route
        //
        // TODO FIXME -- with this commented out exit mode cannot work!!
        //
        log::critical(logcat, "FIXME TODO: not adding default route yet because ???");
        // const auto ep = router.hidden_service_context().GetDefault();
        // if (auto* vpn = ep->GetVPNInterface())
        //   route.add_default_route_via_interface(*vpn);
        log::info(logcat, "route poker up");

        // TODO FIXME
        // set_dns_mode(true);
    }

    void RoutePoker::put_down()
    {
        if (!_up)
            return;

        // unpoke routes for first hops
        _router.for_each_connection([this](const RouterID&, link::Connection& conn) {
            auto remote = conn.conn->remote();
            if (remote.is_ipv4())
                delete_route(remote.to_ipv4());
            else
                delete_route(remote.to_ipv6());
        });

        if (_enabled)
        {
            // TODO FIXME
            // vpn::AbstractRouteManager& route = router.vpn_platform()->RouteManager();
            // const auto ep = router.hidden_service_context().GetDefault();
            // if (auto* vpn = ep->GetVPNInterface())
            //   route.delete_default_route_via_interface(*vpn);

            // delete route blackhole
            // route.delete_blackhole();
            // log::info(logcat, "route poker down");
        }

        // set_dns_mode(false);
        _up = false;
    }

}  // namespace llarp
