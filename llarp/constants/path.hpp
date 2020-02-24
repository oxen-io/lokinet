#ifndef LLARP_CONSTANTS_PATH_HPP
#define LLARP_CONSTANTS_PATH_HPP

#include <cstddef>

#include <util/types.hpp>
#include <util/time.hpp>

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
    static constexpr auto default_lifetime = 10min;
    /// after this many ms a path build times out
    static constexpr auto build_timeout = 30s;

    /// measure latency every this interval ms
    static constexpr auto latency_interval = 5s;
    /// if a path is inactive for this amount of time it's dead
    static constexpr auto alive_timeout = 30s;
  }  // namespace path
}  // namespace llarp

#endif
