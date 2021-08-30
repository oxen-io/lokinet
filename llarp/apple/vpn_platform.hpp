#pragma once

#include <llarp/ev/vpn.hpp>
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
        void* callback_context)
        : m_Context{ctx}
        , m_RouteManager{std::move(route_callbacks), callback_context}
        , m_PacketWriter{std::move(packet_writer)}
        , m_OnReadable{std::move(on_readable)}
    {}

    std::shared_ptr<vpn::NetworkInterface> ObtainInterface(vpn::InterfaceInfo) override
    {
      return std::make_shared<VPNInterface>(m_Context, m_PacketWriter, m_OnReadable);
    }

    vpn::IRouteManager& RouteManager() override { return m_RouteManager; }

   private:
    Context& m_Context;
    apple::RouteManager m_RouteManager;
    VPNInterface::packet_write_callback m_PacketWriter;
    VPNInterface::on_readable_callback m_OnReadable;
  };

}  // namespace llarp::apple
