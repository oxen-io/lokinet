#ifndef LLARP_TIME_HPP
#define LLARP_TIME_HPP

#include <util/types.hpp>
#include <chrono>

#include <chrono>

namespace llarp
{
  /// get time right now as milliseconds, this is monotonic
  llarp_time_t
  time_now_ms();

  using Time_t = std::chrono::milliseconds;

  /// get time right now as a Time_t, monotonic
  Time_t
  time_now();

}  // namespace llarp

#endif
