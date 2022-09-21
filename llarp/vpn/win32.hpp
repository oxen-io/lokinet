#pragma once

#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/win32/exec.hpp>
#include <llarp.hpp>

#include "platform.hpp"

namespace llarp::win32
{
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
    Route(std::string ip, std::string gw, std::string cmd);

    void
    DefaultRouteViaInterface(NetworkInterface& vpn, std::string cmd);

    void
    RouteViaInterface(NetworkInterface& vpn, std::string addr, std::string mask, std::string cmd);

   public:
    VPNPlatform(const VPNPlatform&) = delete;
    VPNPlatform(VPNPlatform&&) = delete;

    VPNPlatform(llarp::Context* ctx) : Platform{}, _ctx{ctx}
    {}

    ~VPNPlatform() override = default;

    void
    AddRoute(net::ipaddr_t ip, net::ipaddr_t gateway) override;

    void
    DelRoute(net::ipaddr_t ip, net::ipaddr_t gateway) override;

    void
    AddRouteViaInterface(NetworkInterface& vpn, IPRange range) override;

    void
    DelRouteViaInterface(NetworkInterface& vpn, IPRange range) override;

    std::vector<net::ipaddr_t>
    GetGatewaysNotOnInterface(NetworkInterface& vpn) override;

    void
    AddDefaultRouteViaInterface(NetworkInterface& vpn) override;

    void
    DelDefaultRouteViaInterface(NetworkInterface& vpn) override;

    std::shared_ptr<NetworkInterface>
    ObtainInterface(InterfaceInfo info, AbstractRouter* router) override;

    std::shared_ptr<I_Packet_IO>
    create_packet_io(
        unsigned int ifindex, const std::optional<SockAddr>& dns_upstream_src) override;

    IRouteManager&
    RouteManager() override
    {
      return *this;
    }
  };

}  // namespace llarp::win32
