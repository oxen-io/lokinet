#ifndef LLARP_TIME_HPP
#define LLARP_TIME_HPP

#include <util/types.hpp>

#include <chrono>

namespace llarp
{
  using Clock_t = std::chrono::system_clock;

  llarp_time_t
  time_now_ms();
}  // namespace llarp

#endif
