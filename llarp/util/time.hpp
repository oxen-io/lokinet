#pragma once

#include "types.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fmt/format.h>
#include <fmt/chrono.h>

using namespace std::chrono_literals;

namespace llarp
{
  /// get the uptime of the process
  Duration_t
  uptime();

  /// get time the process started at
  TimePoint_t
  started_at();

  /// convert to milliseconds unix stamp
  uint64_t
  to_unix_stamp(const Duration_t& duration);

  nlohmann::json
  to_json(const Duration_t& t);

  template <typename delta_res_t = std::chrono::milliseconds>
  auto
  time_until(const TimePoint_t& now, const TimePoint_t& other)
  {
    return std::chrono::duration_cast<delta_res_t>(other - now);
  }

  /// returns the time now as a time point, this is monotonic
  inline TimePoint_t
  time_now()
  {
    return started_at() + uptime();
  }

  /// convert a timepoint to a unix millis stamp
  uint64_t
  to_unix_stamp(const TimePoint_t& t);

  /// get time right now as milliseconds, (legacy wrapper)
  inline Duration_t
  time_now_ms()
  {
    return Duration_t{to_unix_stamp(time_now())};
  }

  /// convert a legacy time stamp to a time point
  template <typename Time_Duration = std::chrono::milliseconds>
  auto
  to_time_point(llarp_time_t stamp)
  {
    const auto now = time_now();
    return now + (stamp - Duration_t{to_unix_stamp(now)});
  }
}  // namespace llarp

namespace fmt
{
  template <>
  struct formatter<llarp::Duration_t> : formatter<std::string>
  {
    template <typename FormatContext>
    auto
    format(llarp::Duration_t elapsed, FormatContext& ctx)
    {
      bool neg = elapsed < 0s;
      if (neg)
        elapsed = -elapsed;
      const auto hours = std::chrono::duration_cast<std::chrono::hours>(elapsed).count();
      const auto mins = (std::chrono::duration_cast<std::chrono::minutes>(elapsed) % 1h).count();
      const auto secs = (std::chrono::duration_cast<std::chrono::seconds>(elapsed) % 1min).count();
      const auto ms = (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed) % 1s).count();
      return formatter<std::string>::format(
          fmt::format(
              elapsed >= 1h         ? "{0}{1:d}h{2:02d}m{3:02d}.{4:03d}s"
                  : elapsed >= 1min ? "{0}{2:d}m{3:02d}.{4:03d}s"
                                    : "{0}{3:d}.{4:03d}s",
              neg ? "-" : "",
              hours,
              mins,
              secs,
              ms),
          ctx);
    }
  };

  template <>
  struct formatter<llarp::TimePoint_t> : formatter<std::string>
  {
    template <typename FormatContext>
    auto
    format(const llarp::TimePoint_t& tp, FormatContext& ctx)
    {
      return formatter<std::string>::format(fmt::format("{:%c %Z}", tp), ctx);
    }
  };
}  // namespace fmt
