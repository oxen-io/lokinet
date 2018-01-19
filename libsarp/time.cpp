#include <sarp/time.h>
#include <chrono>

namespace sarp
{
  typedef std::chrono::steady_clock clock_t;

  template<typename Res>
  static uint64_t time_since_epoch()
  {
    return std::chrono::duration_cast<Res>(sarp::clock_t::now().time_since_epoch()).count();
  }
}

extern "C" {
  uint64_t sarp_time_now_ms()
  {
    return sarp::time_since_epoch<std::chrono::milliseconds>();
  }
  
  uint64_t sarp_time_now_sec()
  {
    return sarp::time_since_epoch<std::chrono::seconds>();
  }
}
