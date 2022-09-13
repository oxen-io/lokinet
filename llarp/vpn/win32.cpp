#include "vpn/win32.hpp"
#include <llarp/win32/windivert.hpp>
#include <llarp/win32/wintun.hpp>
#include <fmt/core.h>

namespace llarp::win32
{
  namespace
  {
    template <typename T>
    std::string
    ip_to_string(T ip)
    {
      return var::visit([](auto&& ip) { return ip.ToString(); }, ip);
    }
  }  // namespace

  void
  VPNPlatform::Route(std::string ip, std::string gw, std::string cmd)
  {
    llarp::win32::Exec(
        "route.exe", fmt::format("{} {} MASK 255.255.255.255 {} METRIC {}", cmd, ip, gw, m_Metric));
  }

  void
  VPNPlatform::DefaultRouteViaInterface(NetworkInterface& vpn, std::string cmd)
  {
    // route hole for loopback bacause god is dead on windows
    llarp::win32::Exec("route.exe", fmt::format("{} 127.0.0.0 MASK 255.0.0.0 0.0.0.0", cmd));
    // set up ipv4 routes
    RouteViaInterface(vpn, "0.0.0.0", "128.0.0.0", cmd);
    RouteViaInterface(vpn, "128.0.0.0", "128.0.0.0", cmd);
  }

  void
  VPNPlatform::RouteViaInterface(
      NetworkInterface& vpn, std::string addr, std::string mask, std::string cmd)
  {
    const auto& info = vpn.Info();
    auto ifaddr = ip_to_string(info[0]);
    // this changes the last 1 to a 0 so that it routes over the interface
    // this is required because windows is idiotic af
    ifaddr.back()--;
    llarp::win32::Exec(
        "route.exe", fmt::format("{} {} MASK {} {} METRIC {}", cmd, addr, mask, ifaddr, m_Metric));
  }

  void
  VPNPlatform::AddRoute(net::ipaddr_t ip, net::ipaddr_t gateway)
  {
    Route(ip_to_string(ip), ip_to_string(gateway), "ADD");
  }

  void
  VPNPlatform::DelRoute(net::ipaddr_t ip, net::ipaddr_t gateway)
  {
    Route(ip_to_string(ip), ip_to_string(gateway), "DELETE");
  }

  void
  VPNPlatform::AddRouteViaInterface(NetworkInterface& vpn, IPRange range)
  {
    RouteViaInterface(vpn, range.BaseAddressString(), range.NetmaskString(), "ADD");
  }

  void
  VPNPlatform::DelRouteViaInterface(NetworkInterface& vpn, IPRange range)
  {
    RouteViaInterface(vpn, range.BaseAddressString(), range.NetmaskString(), "DELETE");
  }

  std::vector<net::ipaddr_t>
  VPNPlatform::GetGatewaysNotOnInterface(NetworkInterface& vpn)
  {
    std::vector<net::ipaddr_t> gateways;

    auto idx = vpn.Info().index;
    using UInt_t = decltype(idx);
    for (const auto& iface : Net().AllNetworkInterfaces())
    {
      if (static_cast<UInt_t>(iface.index) == idx)
        continue;
      if (iface.gateway)
        gateways.emplace_back(*iface.gateway);
    }
    return gateways;
  }

  void
  VPNPlatform::AddDefaultRouteViaInterface(NetworkInterface& vpn)
  {
    // kill ipv6
    llarp::win32::Exec(
        "WindowsPowerShell\\v1.0\\powershell.exe",
        "-Command (Disable-NetAdapterBinding -Name \"* \" -ComponentID ms_tcpip6)");

    DefaultRouteViaInterface(vpn, "ADD");
  }

  void
  VPNPlatform::DelDefaultRouteViaInterface(NetworkInterface& vpn)
  {
    // restore ipv6
    llarp::win32::Exec(
        "WindowsPowerShell\\v1.0\\powershell.exe",
        "-Command (Enable-NetAdapterBinding -Name \"* \" -ComponentID ms_tcpip6)");

    DefaultRouteViaInterface(vpn, "DELETE");
  }

  std::shared_ptr<NetworkInterface>
  VPNPlatform::ObtainInterface(InterfaceInfo info, AbstractRouter* router)
  {
    return wintun::make_interface(std::move(info), router);
  }

  std::shared_ptr<I_Packet_IO>
  VPNPlatform::create_packet_io(
      unsigned int ifindex, const std::optional<SockAddr>& dns_upstream_src)
  {
    // we only want do this on all interfaes with windivert
    if (ifindex)
      throw std::invalid_argument{
          "cannot create packet io on explicitly specified interface, not currently supported on "
          "windows (yet)"};

    uint16_t upstream_src_port = dns_upstream_src ? dns_upstream_src->getPort() : 0;
    std::string udp_filter = upstream_src_port != 0
        ? fmt::format("( udp.DstPort == 53 and udp.SrcPort != {} )", upstream_src_port)
        : "udp.DstPort == 53";

    auto filter = "outbound and ( " + udp_filter + " or tcp.DstPort == 53 )";

    return WinDivert::make_interceptor(filter, [router = _ctx->router] { router->TriggerPump(); });
  }
}  // namespace llarp::win32
