
#include "vpn_interface.hpp"
#include "context.hpp"
#include <llarp/router/abstractrouter.hpp>

namespace llarp::apple
{
  VPNInterface::VPNInterface(
      Context& ctx,
      packet_write_callback packet_writer,
      on_readable_callback on_readable,
      AbstractRouter* router)
      : vpn::NetworkInterface{{}}
      , m_PacketWriter{std::move(packet_writer)}
      , m_OnReadable{std::move(on_readable)}
      , _router{router}
  {
    ctx.loop->call_soon([this] { m_OnReadable(*this); });
  }

  bool
  VPNInterface::OfferReadPacket(const llarp_buffer_t& buf)
  {
    llarp::net::IPPacket pkt;
    if (!pkt.Load(buf))
      return false;
    m_ReadQueue.tryPushBack(std::move(pkt));
    return true;
  }

  void
  VPNInterface::MaybeWakeUpperLayers() const
  {
    _router->TriggerPump();
  }

  int
  VPNInterface::PollFD() const
  {
    return -1;
  }

  net::IPPacket
  VPNInterface::ReadNextPacket()
  {
    net::IPPacket pkt{};
    if (not m_ReadQueue.empty())
      pkt = m_ReadQueue.popFront();
    return pkt;
  }

  bool
  VPNInterface::WritePacket(net::IPPacket pkt)
  {
    int af_family = pkt.IsV6() ? AF_INET6 : AF_INET;
    return m_PacketWriter(af_family, pkt.data(), pkt.size());
  }

}  // namespace llarp::apple
