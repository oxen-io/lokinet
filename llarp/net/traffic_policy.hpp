#pragma once

#include "ip_packet.hpp"
#include "ip_range.hpp"

#include <llarp/util/status.hpp>

#include <oxenc/bt.h>

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

    ProtocolInfo(std::string buf);

    void
    bt_encode(oxenc::bt_list_producer& btlp) const;

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
      return std::tie(protocol, port) < std::tie(other.protocol, other.port);
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

    void
    bt_encode(oxenc::bt_dict_producer& btdp) const;
    void
    bt_decode(oxenc::bt_dict_consumer& btdc);

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
