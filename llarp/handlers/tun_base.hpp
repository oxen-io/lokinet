#pragma once

#include <llarp/address/address.hpp>
#include <llarp/address/types.hpp>
#include <llarp/contact/tag.hpp>
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

        virtual std::optional<ipv4> map_session_to_local_ip(const NetworkAddress& remote) = 0;
        virtual void unmap_session_to_local_ip(const NetworkAddress& remote) = 0;

        virtual void handle_inbound_packet(IPPacket pkt, session_tag tag, NetworkAddress remote) = 0;
    };

}  // namespace llarp::handlers
