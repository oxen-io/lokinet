#pragma once
#include <llarp/net/net_int.hpp>
#include <llarp/net/ip_packet.hpp>
#include <functional>
#include <unordered_map>

namespace llarp::vpn
{
  using PacketHandlerFunc = std::function<void(llarp::net::IPPacket)>;

  struct Layer4Handler
  {
    virtual ~Layer4Handler() = default;

    virtual void
    HandleIPPacket(llarp::net::IPPacket pkt) = 0;

    virtual void AddSubHandler(nuint16_t, PacketHandlerFunc){};
  };

  class PacketRouter
  {
    PacketHandlerFunc m_BaseHandler;
    std::unordered_map<uint8_t, std::unique_ptr<Layer4Handler>> m_IPProtoHandler;

   public:
    /// baseHandler will be called if no other handlers matches a packet
    explicit PacketRouter(PacketHandlerFunc baseHandler);

    /// feed in an ip packet for handling
    void
    HandleIPPacket(llarp::net::IPPacket pkt);

    /// add a non udp packet handler using ip protocol proto
    void
    AddIProtoHandler(uint8_t proto, PacketHandlerFunc func);

    /// helper that adds a udp packet handler for UDP destinted for localport
    void
    AddUDPHandler(huint16_t localport, PacketHandlerFunc func);
  };
}  // namespace llarp::vpn
