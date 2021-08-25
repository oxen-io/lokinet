#pragma once

#include <llarp/ev/vpn.hpp>
#include "vpn_interface.hpp"

namespace llarp::apple
{
  class VPNPlatform final : public vpn::Platform
  {
   public:
    explicit VPNPlatform(
        Context& ctx,
        VPNInterface::packet_write_callback packet_writer,
        VPNInterface::on_readable_callback on_readable)
        : m_Context{ctx}
        , m_PacketWriter{std::move(packet_writer)}
        , m_OnReadable{std::move(on_readable)}
    {}

    std::shared_ptr<vpn::NetworkInterface> ObtainInterface(vpn::InterfaceInfo) override
    {
      return std::make_shared<VPNInterface>(m_Context, m_PacketWriter, m_OnReadable);
    }

   private:
    Context& m_Context;
    VPNInterface::packet_write_callback m_PacketWriter;
    VPNInterface::on_readable_callback m_OnReadable;
  };

}  // namespace llarp::apple
