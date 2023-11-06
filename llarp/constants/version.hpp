#pragma once

#include <array>
#include <cstdint>

namespace llarp
{
  // Given a full lokinet version of: lokinet-1.2.3-abc these are:
  extern const std::array<uint16_t, 3> LOKINET_VERSION;
  extern const char* const LOKINET_VERSION_TAG;
  extern const char* const LOKINET_VERSION_FULL;

  extern const char* const LOKINET_RELEASE_MOTTO;
  extern const char* const LOKINET_DEFAULT_NETID;
}  // namespace llarp
