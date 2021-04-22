#pragma once

#include "ip_range.hpp"
#include "ip_packet.hpp"
#include "llarp/util/status.hpp"

#include <set>

namespace llarp::net
{
  /// information about an IP protocol
  struct ProtocolInfo
  {
    /// ip protocol byte of this protocol
    IPProtocol protocol;
    /// the layer 3 port if applicable
    std::optional<nuint16_t> port;

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    BDecode(llarp_buffer_t* buf);

    util::StatusObject
    ExtractStatus() const;

    /// returns true if an ip packet looks like it matches this protocol info
    /// returns false otherwise
    bool
    MatchesPacket(const IPPacket& pkt) const;

    bool
    operator<(const ProtocolInfo& other) const
    {
      if (port and other.port)
      {
        return protocol < other.protocol or *port < *other.port;
      }
      return protocol < other.protocol;
    }

    ProtocolInfo() = default;

    explicit ProtocolInfo(std::string_view spec);
  };

  /// information about what traffic an endpoint will carry
  struct TrafficPolicy
  {
    /// ranges that are explicitly allowed
    std::set<IPRange> ranges;

    /// protocols that are explicity allowed
    std::set<ProtocolInfo> protocols;

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    BDecode(llarp_buffer_t* buf);
    util::StatusObject
    ExtractStatus() const;

    /// returns true if we allow the traffic in this ip packet
    /// returns false otherwise
    bool
    AllowsTraffic(const IPPacket& pkt) const;
  };
}  // namespace llarp::net
