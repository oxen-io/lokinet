#pragma once

#include <llarp/vpn/platform.hpp>
#include "vpn_interface.hpp"
#include "route_manager.hpp"

namespace llarp::apple
{
  class VPNPlatform final : public vpn::Platform
  {
   public:
    explicit VPNPlatform(
        Context& ctx,
        VPNInterface::packet_write_callback packet_writer,
        VPNInterface::on_readable_callback on_readable,
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
    VPNInterface::packet_write_callback m_PacketWriter;
    VPNInterface::on_readable_callback m_OnReadable;
  };

}  // namespace llarp::apple
