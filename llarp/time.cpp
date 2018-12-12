#include <time.hpp>

namespace llarp
{
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
    return llarp::time_since_epoch< std::chrono::milliseconds >();
  }
}  // namespace llarp
