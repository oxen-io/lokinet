#pragma once

#include <chrono>

namespace llarp
{
  using namespace std::literals;
  /// how big of a time skip before we reset network state
  constexpr auto TimeskipDetectedDuration = 1min;
}  // namespace llarp
