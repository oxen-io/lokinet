#pragma once

#include <cstdint>

namespace llarp::service
{
  using ProtocolType = uint64_t;

  constexpr ProtocolType eProtocolControl = 0UL;
  constexpr ProtocolType eProtocolTrafficV4 = 1UL;
  constexpr ProtocolType eProtocolTrafficV6 = 2UL;
  constexpr ProtocolType eProtocolExit = 3UL;
  constexpr ProtocolType eProtocolAuth = 4UL;
}  // namespace llarp::service
