#include <llarp/time.h>
#include <chrono>

namespace llarp
{
  typedef std::chrono::steady_clock clock_t;

  template<typename Res>
  static uint64_t time_since_epoch()
  {
    return std::chrono::duration_cast<Res>(llarp::clock_t::now().time_since_epoch()).count();
  }
}

extern "C" {
  uint64_t llarp_time_now_ms()
  {
    return llarp::time_since_epoch<std::chrono::milliseconds>();
  }
  
  uint64_t llarp_time_now_sec()
  {
    return llarp::time_since_epoch<std::chrono::seconds>();
  }
}
