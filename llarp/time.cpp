#include <llarp/time.h>
#include <chrono>

namespace llarp
{
  typedef std::chrono::steady_clock clock_t;

  template < typename Res, typename IntType >
  static IntType
  time_since_epoch()
  {
    return std::chrono::duration_cast< Res >(
               llarp::clock_t::now().time_since_epoch())
        .count();
  }
}  // namespace llarp

llarp_time_t
llarp_time_now_ms()
{
  return llarp::time_since_epoch< std::chrono::milliseconds, llarp_time_t >();
}

llarp_seconds_t
llarp_time_now_sec()
{
  return llarp::time_since_epoch< std::chrono::seconds, llarp_seconds_t >();
}
