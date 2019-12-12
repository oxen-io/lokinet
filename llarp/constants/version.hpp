#pragma once

#include <cstdint>
#include <array>

namespace llarp
{
  // Given a full lokinet version of: lokinet-1.2.3-abc these are:
  extern const std::array<uint16_t, 3> VERSION; // [1, 2, 3]
  extern const char* const VERSION_STR; // "1.2.3"
  extern const char* const VERSION_TAG; // "abc"
  extern const char* const VERSION_FULL; // "lokinet-1.2.3-abc"

  extern const char* const RELEASE_MOTTO;
  extern const char* const DEFAULT_NETID;
}
