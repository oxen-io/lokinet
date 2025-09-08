#pragma once

#include <llarp/address/address.hpp>
#include <llarp/address/types.hpp>
#include <llarp/net/ip_packet.hpp>

namespace llarp::handlers
{

    // Abstract class for TUN handling.  This base interface exists so that embedded clients can be
    // built without needing to compile any tun code at all.
    class TunEPBase
    {
      public:
        virtual ~TunEPBase() = default;

        virtual void start_poller() = 0;

        virtual std::optional<ipv4> map(const NetworkAddress& remote) = 0;
        virtual void unmap(const NetworkAddress& remote) = 0;

        virtual void handle_inbound_packet(IPPacket pkt, uint8_t type, NetworkAddress remote) = 0;
    };

}  // namespace llarp::handlers
