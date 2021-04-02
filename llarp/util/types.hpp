#pragma once
#include <cstdint>
#include <string>
#include <chrono>

using byte_t = uint8_t;
using llarp_proto_version_t = std::uint8_t;

namespace llarp
{
  using Duration_t = std::chrono::milliseconds;
  using namespace std::literals;

  /// convert to milliseconds
  uint64_t
  ToMS(Duration_t duration);
}  // namespace llarp

using llarp_time_t = llarp::Duration_t;
