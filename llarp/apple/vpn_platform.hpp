#pragma once

#include "vpn_interface.hpp"
#include "route_manager.hpp"

namespace llarp::apple
{
  class VPNPlatform final : public vpn::Platform
  {
   public:
    explicit VPNPlatform(
        Context& ctx,
        packet_write_callback packet_writer,
        llarp_route_callbacks route_callbacks,
        void* callback_context);

    std::shared_ptr<vpn::NetworkInterface>
    ObtainInterface(vpn::InterfaceInfo, AbstractRouter*) override;

    vpn::IRouteManager&
    RouteManager() override
    {
      return m_RouteManager;
    }

   private:
    Context& m_Context;
    apple::RouteManager m_RouteManager;
    packet_write_callback m_PacketWriter;
  };

}  // namespace llarp::apple
