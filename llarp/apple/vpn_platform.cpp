#include "vpn_platform.hpp"
#include "context.hpp"
#include "llarp/layers/platform/platform_layer.hpp"
#include "llarp/net/ip_packet.hpp"

namespace llarp::apple
{

  class VPNInterface : public vpn::QueuedNetworkInterface
  {
    packet_write_callback _write_packet;

   public:
    VPNInterface(
        Context& ctx,
        const layers::platform::PlatformLayer& plat,
        packet_write_callback write_packet)
        : vpn::QueuedNetworkInterface{vpn::InterfaceInfo{}, plat.wakeup_send, plat.wakeup_recv}
        , _write_packet{std::move(write_packet)}
    {
      _recv_wakeup->Trigger();
    }

    bool
    WritePacket(net::IPPacket pkt) override
    {
      return _write_packet(pkt.family(), pkt.data(), static_cast<int>(pkt.size()));
    }
  };

  VPNPlatform::VPNPlatform(
      Context& ctx,
      packet_write_callback packet_writer,
      llarp_route_callbacks route_callbacks,
      void* callback_context)
      : m_Context{ctx}
      , m_RouteManager{ctx, std::move(route_callbacks), callback_context}
      , m_PacketWriter{std::move(packet_writer)}
  {}

  std::shared_ptr<vpn::NetworkInterface>
  VPNPlatform::ObtainInterface(vpn::InterfaceInfo, AbstractRouter* router)
  {
    const auto& plat = *router->get_layers()->platform;
    return std::make_shared<VPNInterface>(m_Context, plat, m_PacketWriter);
  }
}  // namespace llarp::apple
