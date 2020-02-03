#ifndef LLARP_TIME_HPP
#define LLARP_TIME_HPP

#include <util/types.hpp>
#include <chrono>

namespace llarp
{
  /// get time right now as milliseconds, this is monotonic
  llarp_time_t
  time_now_ms();
}  // namespace llarp

#endif
