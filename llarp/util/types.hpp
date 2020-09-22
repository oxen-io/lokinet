#ifndef LLARP_TYPES_H
#define LLARP_TYPES_H
#include <cstdint>
#include <string>
#include <chrono>

using byte_t = uint8_t;
using llarp_proto_version_t = std::uint8_t;

using llarp_time_t = std::chrono::milliseconds;

namespace llarp
{
  using namespace std::literals;
}

#endif
