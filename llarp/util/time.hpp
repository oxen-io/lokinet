#pragma once

#include "types.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fmt/format.h>
#include <fmt/chrono.h>

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

  nlohmann::json
  to_json(const Duration_t& t);

  template <typename Time_Duration>
  struct time_delta
  {
    const TimePoint_t at;

    /// get the time delta between this time point and now
    auto
    delta() const
    {
      return std::chrono::duration_cast<Time_Duration>(llarp::TimePoint_t::clock::now() - at);
    }
  };
}  // namespace llarp

namespace fmt
{
  template <typename Time_Duration>
  struct formatter<llarp::time_delta<Time_Duration>> : formatter<std::string>
  {
    template <typename FormatContext>
    auto
    format(const llarp::time_delta<Time_Duration>& td, FormatContext& ctx)
    {
      const auto dlt = td.delta();
      using Parent = formatter<std::string>;
      if (dlt > 0s)
        return Parent::format(fmt::format("{} ago", dlt), ctx);
      if (dlt < 0s)
        return Parent::format(fmt::format("in {}", -dlt), ctx);
      return Parent::format("now", ctx);
    }
  };

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
