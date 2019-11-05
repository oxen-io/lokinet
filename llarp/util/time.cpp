#include <util/time.hpp>
#include <chrono>
#include <util/logging/logger.hpp>

namespace llarp
{
  using Clock_t = std::chrono::system_clock;

  template < typename Res >
  static llarp_time_t
  time_since_epoch()
  {
    return std::chrono::duration_cast< Res >(
               llarp::Clock_t::now().time_since_epoch())
        .count();
  }

  // use std::chrono because otherwise the network breaks with Daylight Savings
  // this time, it doesn't get truncated -despair
  // that concern is what drove me back to the POSIX C time functions
  // in the first place
  llarp_time_t
  time_now_ms()
  {
    static llarp_time_t lastTime = 0;
    auto t = llarp::time_since_epoch< std::chrono::milliseconds >();
    if(t <= lastTime)
    {
      return lastTime;
    }
    if(lastTime == 0)
    {
      lastTime = t;
    }
    const auto dlt = t - lastTime;
    if(dlt > 5000)
    {
      // big timeskip
      t        = lastTime;
      lastTime = 0;
    }
    else
    {
      lastTime = t;
    }
    return t;
  }
}  // namespace llarp
