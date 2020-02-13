#include <util/time.hpp>
#include <chrono>
#include <util/logging/logger.hpp>

namespace llarp
{
  using Clock_t = std::chrono::system_clock;

  template < typename Res, typename Clock >
  static llarp_time_t
  time_since_epoch()
  {
    return std::chrono::duration_cast< Res >(Clock::now().time_since_epoch())
        .count();
  }

  const static llarp_time_t started_at_system =
      time_since_epoch< std::chrono::milliseconds, Clock_t >();

  const static llarp_time_t started_at_steady =
      time_since_epoch< std::chrono::milliseconds,
                        std::chrono::steady_clock >();
  /// get our uptime in ms
  static llarp_time_t
  time_since_started()
  {
    return time_since_epoch< std::chrono::milliseconds,
                             std::chrono::steady_clock >()
        - started_at_steady;
  }

  Time_t
  time_now()
  {
    return Time_t(time_now_ms());
  }

  llarp_time_t
  time_now_ms()
  {
    static llarp_time_t lastTime = 0;
    auto t                       = time_since_started();
#ifdef TESTNET_SPEED
    t /= TESTNET_SPEED;
#endif
    t += started_at_system;

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
