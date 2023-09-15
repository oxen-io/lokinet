#include "vpn_platform.hpp"
#include "context.hpp"

namespace llarp::apple
{
  VPNPlatform::VPNPlatform(
      Context& ctx,
      VPNInterface::packet_write_callback packet_writer,
      VPNInterface::on_readable_callback on_readable,
      llarp_route_callbacks route_callbacks,
      void* callback_context)
      : m_Context{ctx}
      , m_RouteManager{ctx, std::move(route_callbacks), callback_context}
      , m_PacketWriter{std::move(packet_writer)}
      , m_OnReadable{std::move(on_readable)}
  {}

  std::shared_ptr<vpn::NetworkInterface>
  VPNPlatform::ObtainInterface(vpn::InterfaceInfo, Router* router)
  {
    return std::make_shared<VPNInterface>(m_Context, m_PacketWriter, m_OnReadable, router);
  }
}  // namespace llarp::apple
