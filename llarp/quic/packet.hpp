#pragma once

#include "connection.hpp"

namespace llarp::quic
{
  // Encapsulates a packet, i.e. the remote addr, packet data, plus metadata.
  struct Packet
  {
    Path path;
    bstring_view data;
    ngtcp2_pkt_info info;
  };

}  // namespace llarp::quic
