#pragma once

#include <cstdint>

namespace llarp::service
{
  using ProtocolType = uint64_t;

  constexpr ProtocolType eProtocolControl = 0UL;
  constexpr ProtocolType eProtocolTrafficV4 = 1UL;
  constexpr ProtocolType eProtocolTrafficV6 = 2UL;

}  // namespace llarp::service
