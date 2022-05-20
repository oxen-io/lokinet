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

  std::ostream&
  operator<<(std::ostream& out, const TimePoint_t& t);

  template <typename Time_Duration>
  struct time_delta
  {
    const TimePoint_t at;

    std::ostream&
    operator()(std::ostream& out) const
    {
      const auto dlt = std::chrono::duration_cast<Time_Duration>(TimePoint_t::clock::now() - at);
      if (dlt > 0s)
        return out << std::chrono::duration_cast<Duration_t>(dlt) << " ago ";
      else if (dlt < 0s)
        return out << "in " << std::chrono::duration_cast<Duration_t>(-dlt);
      else
        return out << "now";
    }
  };

  inline std::ostream&
  operator<<(std::ostream& out, const time_delta<std::chrono::seconds>& td)
  {
    return td(out);
  }

  inline std::ostream&
  operator<<(std::ostream& out, const time_delta<std::chrono::milliseconds>& td)
  {
    return td(out);
  }
}  // namespace llarp
