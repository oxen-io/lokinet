#include <llarp/time.h>
#include <llarp/time.hpp>

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

}  // namespace llarp

// use std::chrono because otherwise the network breaks with Daylight Savings
llarp_time_t
llarp_time_now_ms()
{
  return llarp::time_since_epoch< std::chrono::milliseconds >();
}
