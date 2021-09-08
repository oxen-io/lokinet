
#include "vpn_interface.hpp"
#include "context.hpp"

namespace llarp::apple
{
  VPNInterface::VPNInterface(
      Context& ctx, packet_write_callback packet_writer, on_readable_callback on_readable)
      : m_PacketWriter{std::move(packet_writer)}, m_OnReadable{std::move(on_readable)}
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

  int
  VPNInterface::PollFD() const
  {
    return -1;
  }

  std::string
  VPNInterface::IfName() const
  {
    return "";
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
    return m_PacketWriter(af_family, pkt.buf, pkt.sz);
  }

}  // namespace llarp::apple
