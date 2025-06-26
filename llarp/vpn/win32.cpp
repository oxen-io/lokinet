#include "win32.hpp"

#include <llarp/win32/adapters.hpp>
#include <llarp/win32/windivert.hpp>
#include <llarp/win32/wintun.hpp>

#include <fmt/core.h>

namespace llarp::win32
{
    namespace
    {
        template <typename T>
        std::string ip_to_string(T ip)
        {
            return var::visit([](auto&& ip) { return ip.to_string(); }, ip);
        }
    }  // namespace

    void VPNPlatform::make_route(std::string ip, std::string gw, std::string cmd)
    {
        llarp::win32::Exec("route.exe", fmt::format("{} {} MASK 255.255.255.255 {} METRIC {}", cmd, ip, gw, m_Metric));
    }

    void VPNPlatform::default_route_via_interface(NetworkInterface& vpn, std::string cmd)
    {
        // route hole for loopback bacause god is dead on windows
        llarp::win32::Exec("route.exe", fmt::format("{} 127.0.0.0 MASK 255.0.0.0 0.0.0.0", cmd));
        // set up ipv4 routes
        route_via_interface(vpn, "0.0.0.0", "128.0.0.0", cmd);
        route_via_interface(vpn, "128.0.0.0", "128.0.0.0", cmd);
    }

    void VPNPlatform::route_via_interface(NetworkInterface& vpn, std::string addr, std::string mask, std::string cmd)
    {
        const auto& info = vpn.interface_info();
        auto ifaddr = ip_to_string(info[0]);
        // this changes the last 1 to a 0 so that it routes over the interface
        // this is required because windows is idiotic af
        ifaddr.back()--;
        llarp::win32::Exec("route.exe", fmt::format("{} {} MASK {} {} METRIC {}", cmd, addr, mask, ifaddr, m_Metric));
    }

    void VPNPlatform::add_route(quic::Address ip, quic::Address gateway)
    {
        make_route(ip.to_string(), gateway.to_string(), "ADD");
    }

    void VPNPlatform::delete_route(quic::Address ip, quic::Address gateway)
    {
        make_route(ip.to_string(), gateway.to_string(), "DELETE");
    }

    void VPNPlatform::add_route_via_interface(NetworkInterface& vpn, IPRange range)
    {
        route_via_interface(vpn, range.BaseAddressString(), range.NetmaskString(), "ADD");
    }

    void VPNPlatform::delete_route_via_interface(NetworkInterface& vpn, IPRange range)
    {
        route_via_interface(vpn, range.BaseAddressString(), range.NetmaskString(), "DELETE");
    }

    std::vector<quic::Address> VPNPlatform::get_non_interface_gateways(NetworkInterface& vpn)
    {
        std::set<quic::Address> gateways;

        // FIXME: This code is probably broken.  The idea here:
        // - iterate through all system interfaces
        // - if the interface has no gateway, skip it.
        // - if any of those interfaces have a network that contains our interface network(s) (that
        //   is: those in the `vpn` input), then skip it.
        // - else collect the gateway address
        //
        win32::iter_adapters([&if_info = vpn.interface_info(), &gateways](auto* a) {
            auto* igw = a->FirstGatewayAddress;
            if (!igw)
                return;
            quic::Address gw{igw->Address.lpSockaddr, igw->Address.iSockaddrLength};

            bool accept = true;
            for (auto* addr = a->FirstUnicastAddress; accept and addr; addr = addr->Next)
            {
                if (addr->Address.lpSockaddr->sa_family != AF_INET && addr->Address.lpSockaddr->sa_family != AF_INET6)
                    continue;
                quic::Address adapter_addr{addr->Address.lpSockaddr, addr->Address.iSockaddrLength};
                auto netmask_bits = ipaddr_netmask_bits(addr->OnLinkPrefixLength, addr->Address.lpSockaddr->sa_family);
                std::variant<ipv4_range, ipv6_range> adapter_range;
                if (adapter_addr.is_ipv4())
                    adapter_range = adapter_addr.to_ipv4() / netmask_bits;
                else
                    adapter_range = adapter_addr.to_ipv6() / netmask_bits;

                for (auto& a : if_info.addrs)
                {
                    auto contains = std::visit(
                        [&adapter_range]<typename Net, typename Range>(const Net& a, const Range& b) {
                            if constexpr (
                                (std::same_as<Net, ipv4_net> && std::same_as<Range, ipv4_range>)
                                || (std::same_as<Net, ipv6_net> && std::same_as<Range, ipv6_range>))
                                return b.contains(a.ip);
                            return false;
                        },
                        a,
                        adapter_range);
                    if (contains)
                    {
                        accept = false;
                        break;
                    }
                }
            }

            if (accept)
                gateways.insert(std::move(gw));
        });

        return {gateways.begin(), gateways.end()};
    }

    void VPNPlatform::add_default_route_via_interface(NetworkInterface& vpn)
    {
        // kill ipv6
        llarp::win32::Exec(
            "WindowsPowerShell\\v1.0\\powershell.exe",
            "-Command (Disable-NetAdapterBinding -Name \"* \" -ComponentID ms_tcpip6)");

        default_route_via_interface(vpn, "ADD");
    }

    void VPNPlatform::delete_default_route_via_interface(NetworkInterface& vpn)
    {
        // restore ipv6
        llarp::win32::Exec(
            "WindowsPowerShell\\v1.0\\powershell.exe",
            "-Command (Enable-NetAdapterBinding -Name \"* \" -ComponentID ms_tcpip6)");

        default_route_via_interface(vpn, "DELETE");
    }

    std::shared_ptr<NetworkInterface> VPNPlatform::obtain_interface(InterfaceInfo info, Router* router)
    {
        return wintun::make_interface(std::move(info), router);
    }

    std::shared_ptr<PacketIO> VPNPlatform::create_packet_io(
        unsigned int ifindex, const std::optional<quic::Address>& dns_upstream_src)
    {
        // we only want do this on all interfaes with windivert
        if (ifindex)
            throw std::invalid_argument{
                "cannot create packet io on explicitly specified interface, not currently "
                "supported on "
                "windows (yet)"};

        uint16_t upstream_src_port = dns_upstream_src ? dns_upstream_src->port() : 0;
        std::string udp_filter = upstream_src_port != 0
            ? fmt::format("( udp.DstPort == 53 and udp.SrcPort != {} )", upstream_src_port)
            : "udp.DstPort == 53";

        auto filter = "outbound and ( " + udp_filter + " or tcp.DstPort == 53 )";

        // TESTNET:
        return WinDivert::make_interceptor(filter, [router = _ctx->router] { /* router->TriggerPump(); */ });
    }
}  // namespace llarp::win32
