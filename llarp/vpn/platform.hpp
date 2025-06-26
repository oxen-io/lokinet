#pragma once

#include "packet_io.hpp"

#include <llarp/address/ip_range.hpp>
#include <llarp/net/ip_packet.hpp>
#include <llarp/net/platform.hpp>

#include <oxen/quic/address.hpp>

#include <variant>

namespace llarp
{
    struct Context;
    class Router;
}  // namespace llarp

namespace llarp::vpn
{
    struct InterfaceInfo
    {
        unsigned int index;
        std::string ifname;
        std::vector<std::variant<ipv4_net, ipv6_net>> addrs;
    };

    /// a vpn network interface
    class NetworkInterface : public PacketIO
    {
      protected:
        InterfaceInfo _info;

      public:
        NetworkInterface() = default;
        NetworkInterface(InterfaceInfo info) : _info{std::move(info)} {}
        NetworkInterface(const NetworkInterface&) = delete;
        NetworkInterface(NetworkInterface&&) = delete;

        const InterfaceInfo& interface_info() const { return _info; }

        /// idempotently wake up the upper layers as needed (platform dependant)
        virtual void MaybeWakeUpperLayers() const {};
    };

    class AbstractRouteManager
    {
      public:
        AbstractRouteManager() = default;
        AbstractRouteManager(const AbstractRouteManager&) = delete;
        AbstractRouteManager(AbstractRouteManager&&) = delete;
        virtual ~AbstractRouteManager() = default;

        virtual const llarp::net::Platform* net_ptr() const;

        inline const llarp::net::Platform& Net() const { return *net_ptr(); }

        virtual void add_route(ipv4 ip, ipv4 gateway) = 0;
        virtual void add_route(ipv6 ip, ipv6 gateway) = 0;

        virtual void delete_route(ipv4 ip, ipv4 gateway) = 0;
        virtual void delete_route(ipv6 ip, ipv6 gateway) = 0;

        virtual void add_default_route_via_interface(NetworkInterface& vpn) = 0;
        virtual void delete_default_route_via_interface(NetworkInterface& vpn) = 0;

        virtual void add_route_via_interface(NetworkInterface& vpn, ipv4_range range) = 0;
        virtual void add_route_via_interface(NetworkInterface& vpn, ipv6_range range) = 0;

        virtual void delete_route_via_interface(NetworkInterface& vpn, ipv4_range range) = 0;
        virtual void delete_route_via_interface(NetworkInterface& vpn, ipv6_range range) = 0;

        virtual std::vector<quic::Address> get_non_interface_gateways(NetworkInterface& vpn) = 0;

        virtual void add_blackhole() {}

        virtual void delete_blackhole() {}
    };

    /// a vpn platform
    /// responsible for obtaining vpn interfaces
    class Platform
    {
      protected:
        /// get a new network interface fully configured given the interface info
        /// blocks until ready, throws on error
        virtual std::shared_ptr<NetworkInterface> obtain_interface(InterfaceInfo info, Router* router) = 0;

      public:
        Platform() = default;
        Platform(const Platform&) = delete;
        Platform(Platform&&) = delete;
        virtual ~Platform() = default;

        /// create and start a network interface
        std::shared_ptr<NetworkInterface> create_interface(InterfaceInfo info, Router* router)
        {
            if (auto netif = obtain_interface(std::move(info), router))
            {
                netif->Start();
                return netif;
            }
            return nullptr;
        }

        /// get owned ip route manager for managing routing table
        virtual AbstractRouteManager& RouteManager() = 0;

        /// create a packet io that will read (and optionally write) packets on a network interface
        /// the lokinet process does not own
        /// @param index the interface index of the network interface to use or 0 for all
        /// interfaces on the system
        virtual std::shared_ptr<PacketIO> create_packet_io(
            [[maybe_unused]] unsigned int ifindex,
            [[maybe_unused]] const std::optional<quic::Address>& dns_upstream_src)
        {
            throw std::runtime_error{"raw packet io is unimplemented"};
        }
    };

    /// create native vpn platform
    std::shared_ptr<Platform> MakeNativePlatform(llarp::Context* ctx);

}  // namespace llarp::vpn
