#pragma once

#include <array>
#include <cstdint>

namespace llarp
{
    // Given a full lokinet version of: lokinet-1.2.3-abc these are:
    extern const std::array<uint8_t, 3> LOKINET_VERSION;  // [1, 2, 3]
    extern const char* const LOKINET_VERSION_TAG;         // "abc"
    extern const char* const LOKINET_VERSION_FULL;        // "lokinet-1.2.3-abc"
}  // namespace llarp
