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

  // Returns a string such as "27m13s ago" or "in 1h12m" or "now".  You get precision of minutes
  // (for >=1h), seconds (>=10s), or milliseconds.  The `now_threshold` argument controls how close
  // to current time (default 1s) the time has to be to get the "now" argument.
  std::string
  short_time_from_now(const TimePoint_t& t, const Duration_t& now_threshold = 1s);

  // Makes a duration human readable.  This always has full millisecond precision, but formats up to
  // hours. E.g. "-4h04m12.123s" or "1234h00m09.876s.
  std::string
  ToString(Duration_t t);

  namespace test
  {
    /// unit test RAII helper type to control the passing of time
    ///
    class TimeDilation
    {
     public:
      TimeDilation();
      ~TimeDilation();

      TimeDilation&
      operator+=(std::chrono::milliseconds amount);
    };
  }  // namespace test
}  // namespace llarp

// Duration_t is currently just a typedef to std::chrono::milliseconds, and specializing
// that seems wrong; leaving this here to remind us not to add it back in again.
// namespace fmt
//{
//  template <>
//  struct formatter<llarp::Duration_t>
//  {
//  };
//}  // namespace fmt
