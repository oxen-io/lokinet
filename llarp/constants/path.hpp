#ifndef LLARP_CONSTANTS_PATH_HPP
#define LLARP_CONSTANTS_PATH_HPP

#include <cstddef>

#include <util/types.hpp>

namespace llarp
{
  namespace path
  {
    /// maximum path length
    constexpr std::size_t max_len = 8;
    /// default path length
    constexpr std::size_t default_len = 4;
    /// pad messages to the nearest this many bytes
    constexpr std::size_t pad_size = 128;
    /// default path lifetime in ms
    constexpr llarp_time_t default_lifetime = 10 * 60 * 1000;
    /// after this many ms a path build times out
    constexpr llarp_time_t build_timeout = 10000;

    /// measure latency every this interval ms
    constexpr llarp_time_t latency_interval = 5000;

    /// if a path is inactive for this amount of time it's dead
    constexpr llarp_time_t alive_timeout = 30000;
  }  // namespace path
}  // namespace llarp

#endif
