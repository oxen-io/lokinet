#pragma once

#include <cstdint>

#include <ostream>

namespace llarp::service
{
  // Supported protocol types; the values are given explicitly because they are specifically used
  // when sending over the wire.
  enum class ProtocolType : uint64_t
  {
    Control = 0UL,
    TrafficV4 = 1UL,
    TrafficV6 = 2UL,
    Exit = 3UL,
    Auth = 4UL,
    QUIC = 5UL,

  };

  std::ostream&
  operator<<(std::ostream& o, ProtocolType t);

}  // namespace llarp::service
