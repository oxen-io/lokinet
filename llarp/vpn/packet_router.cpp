#include "packet_router.hpp"

namespace llarp::vpn
{
  struct UDPPacketHandler : public Layer4Handler
  {
    PacketHandlerFunc m_BaseHandler;
    std::unordered_map<nuint16_t, PacketHandlerFunc> m_LocalPorts;

    explicit UDPPacketHandler(PacketHandlerFunc baseHandler) : m_BaseHandler{std::move(baseHandler)}
    {}

    void
    AddSubHandler(nuint16_t localport, PacketHandlerFunc handler) override
    {
      m_LocalPorts.emplace(localport, std::move(handler));
    }

    void
    HandleIPPacket(llarp::net::IPPacket pkt) override
    {
      const uint8_t* ptr = pkt.buf + (pkt.Header()->ihl * 4) + 2;
      const nuint16_t dstPort{*reinterpret_cast<const uint16_t*>(ptr)};
      if (auto itr = m_LocalPorts.find(dstPort); itr != m_LocalPorts.end())
      {
        itr->second(std::move(pkt));
      }
      else
        m_BaseHandler(std::move(pkt));
    }
  };

  struct GenericLayer4Handler : public Layer4Handler
  {
    PacketHandlerFunc m_BaseHandler;

    explicit GenericLayer4Handler(PacketHandlerFunc baseHandler)
        : m_BaseHandler{std::move(baseHandler)}
    {}

    void
    HandleIPPacket(llarp::net::IPPacket pkt) override
    {
      m_BaseHandler(std::move(pkt));
    }
  };

  PacketRouter::PacketRouter(PacketHandlerFunc baseHandler) : m_BaseHandler{std::move(baseHandler)}
  {}

  void
  PacketRouter::HandleIPPacket(llarp::net::IPPacket pkt)
  {
    const auto proto = pkt.Header()->protocol;
    if (const auto itr = m_IPProtoHandler.find(proto); itr != m_IPProtoHandler.end())
    {
      itr->second->HandleIPPacket(std::move(pkt));
    }
    else
      m_BaseHandler(std::move(pkt));
  }

  void
  PacketRouter::AddUDPHandler(huint16_t localport, PacketHandlerFunc func)
  {
    constexpr byte_t udp_proto = 0x11;

    if (m_IPProtoHandler.find(udp_proto) == m_IPProtoHandler.end())
    {
      m_IPProtoHandler.emplace(udp_proto, std::make_unique<UDPPacketHandler>(m_BaseHandler));
    }
    m_IPProtoHandler[udp_proto]->AddSubHandler(ToNet(localport), func);
  }

  void
  PacketRouter::AddIProtoHandler(uint8_t proto, PacketHandlerFunc func)
  {
    m_IPProtoHandler[proto] = std::make_unique<GenericLayer4Handler>(std::move(func));
  }

}  // namespace llarp::vpn
