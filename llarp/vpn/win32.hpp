#pragma once

#include <windows.h>
#include <iphlpapi.h>
#include <io.h>
#include <fcntl.h>
#include <llarp/util/thread/queue.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/win32/exec.hpp>
#include <llarp/win32/windivert.hpp>
#include <llarp/win32/wintun.hpp>
#include <llarp.hpp>
#include <fmt/std.h>

#include "platform.hpp"

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

  using namespace llarp::vpn;
  class VPNPlatform : public Platform, public IRouteManager
  {
    llarp::Context* const _ctx;
    const int m_Metric{2};

    const auto&
    Net() const
    {
      return _ctx->router->Net();
    }

    void
    Route(std::string ip, std::string gw, std::string cmd)
    {
      llarp::win32::Exec(
          "route.exe",
          fmt::format("{} {} MASK 255.255.255.255 {} METRIC {}", cmd, ip, gw, m_Metric));
    }

    void
    DefaultRouteViaInterface(NetworkInterface& vpn, std::string cmd)
    {
      // route hole for loopback bacause god is dead on windows
      llarp::win32::Exec(
          "route.exe", fmt::format("{} 127.0.0.0 MASK 255.0.0.0 0.0.0.0 METRIC {}", cmd, m_Metric));
      // set up ipv4 routes
      auto lower = RouteViaInterface(vpn, "0.0.0.0", "128.0.0.0", cmd);
      auto upper = RouteViaInterface(vpn, "128.0.0.0", "128.0.0.0", cmd);
    }

    OneShotExec
    RouteViaInterface(NetworkInterface& vpn, std::string addr, std::string mask, std::string cmd)
    {
      const auto& info = vpn.Info();
      auto index = info.index;
      if (index == 0)
      {
        if (auto maybe_idx = net::Platform::Default_ptr()->GetInterfaceIndex(info[0]))
          index = *maybe_idx;
      }

      auto ifaddr = ip_to_string(info[0]);
      // this changes the last 1 to a 0 so that it routes over the interface
      // this is required because windows is idiotic af
      ifaddr.back()--;
      if (index)
      {
        return OneShotExec{
            "route.exe",
            fmt::format(
                "{} {} MASK {} {} IF {} METRIC {}", cmd, addr, mask, ifaddr, info.index, m_Metric)};
      }
      else
      {
        return OneShotExec{
            "route.exe",
            fmt::format("{} {} MASK {} {} METRIC {}", cmd, addr, mask, ifaddr, m_Metric)};
      }
    }

   public:
    VPNPlatform(const VPNPlatform&) = delete;
    VPNPlatform(VPNPlatform&&) = delete;

    VPNPlatform(llarp::Context* ctx) : Platform{}, _ctx{ctx}
    {}

    virtual ~VPNPlatform() = default;

    void
    AddRoute(net::ipaddr_t ip, net::ipaddr_t gateway) override
    {
      Route(ip_to_string(ip), ip_to_string(gateway), "ADD");
    }

    void
    DelRoute(net::ipaddr_t ip, net::ipaddr_t gateway) override
    {
      Route(ip_to_string(ip), ip_to_string(gateway), "DELETE");
    }

    void
    AddRouteViaInterface(NetworkInterface& vpn, IPRange range) override
    {
      RouteViaInterface(vpn, range.BaseAddressString(), range.NetmaskString(), "ADD");
    }

    void
    DelRouteViaInterface(NetworkInterface& vpn, IPRange range) override
    {
      RouteViaInterface(vpn, range.BaseAddressString(), range.NetmaskString(), "DELETE");
    }

    std::vector<net::ipaddr_t>
    GetGatewaysNotOnInterface(NetworkInterface& vpn) override
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
    AddDefaultRouteViaInterface(NetworkInterface& vpn) override
    {
      // kill ipv6
      llarp::win32::Exec(
          "WindowsPowerShell\\v1.0\\powershell.exe",
          "-Command (Disable-NetAdapterBinding -Name \"* \" -ComponentID ms_tcpip6)");

      DefaultRouteViaInterface(vpn, "ADD");
      llarp::win32::Exec("ipconfig.exe", "/flushdns");
    }

    void
    DelDefaultRouteViaInterface(NetworkInterface& vpn) override
    {
      // restore ipv6
      llarp::win32::Exec(
          "WindowsPowerShell\\v1.0\\powershell.exe",
          "-Command (Enable-NetAdapterBinding -Name \"* \" -ComponentID ms_tcpip6)");

      DefaultRouteViaInterface(vpn, "DELETE");
      llarp::win32::Exec("netsh.exe", "winsock reset");
      llarp::win32::Exec("ipconfig.exe", "/flushdns");
    }

    std::shared_ptr<NetworkInterface>
    ObtainInterface(InterfaceInfo info, AbstractRouter* router) override
    {
      return wintun::make_interface(std::move(info), router);
    }

    std::shared_ptr<I_Packet_IO>
    create_packet_io(unsigned int ifindex) override
    {
      // we only want do this on all interfaes with windivert
      if (ifindex)
        throw std::invalid_argument{
            "cannot create packet io on explicitly specified interface, not currently supported on "
            "windows (yet)"};
      return WinDivert::make_interceptor(
          "outbound and ( udp.DstPort == 53 or tcp.DstPort == 53 )",
          [router = _ctx->router] { router->TriggerPump(); });
    }

    IRouteManager&
    RouteManager() override
    {
      return *this;
    }
  };

}  // namespace llarp::win32
