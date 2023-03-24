
#include "vpn_interface.hpp"
#include "context.hpp"
#include "llarp/net/ip_packet.hpp"
#include "llarp/vpn/platform.hpp"
#include <cstddef>
#include <llarp/router/abstractrouter.hpp>

namespace llarp::apple
{
  AppleVPNInterface::AppleVPNInterface(
      Context& ctx, layers::platform::PlatformLayer& plat, packet_write_callback packet_writer)
      : vpn::QueuedNetworkInterface{vpn::InterfaceInfo{}, plat.wakeup_send, plat.wakeup_recv}
      , _apple_ctx{ctx}
      , _write_packet{std::move(packet_writer)}
  {
    // make the reads happen.
    ctx.loop->call_soon([this]() { on_readable(); });
  }

  void
  AppleVPNInterface::on_readable()
  {
    _apple_ctx.on_readable(*this);
  }

  bool
  AppleVPNInterface::WritePacket(net::IPPacket pkt)
  {
    return _write_packet(pkt.family(), pkt.data(), pkt.size());
  }

}  // namespace llarp::apple
