#pragma once

#include "types.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

using namespace std::chrono_literals;

namespace llarp
{
  /// get time right now as milliseconds, this is monotonic
  Duration_t
  time_now_ms();

  /// get the uptime of the process
  Duration_t
  uptime();

  /// convert to milliseconds
  uint64_t
  ToMS(Duration_t duration);

  std::ostream&
  operator<<(std::ostream& out, const Duration_t& t);

  nlohmann::json
  to_json(const Duration_t& t);

}  // namespace llarp
