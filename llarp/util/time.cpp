#include "time.hpp"
#include <chrono>
#include <iomanip>

namespace llarp
{
  namespace
  {
    using Clock_t = std::chrono::system_clock;

    template <typename Res, typename Clock>
    static Duration_t
    time_since_epoch(std::chrono::time_point<Clock> point)
    {
      return std::chrono::duration_cast<Res>(point.time_since_epoch());
    }

    const static auto started_at_system = Clock_t::now();

    const static auto started_at_steady = std::chrono::steady_clock::now();
  }  // namespace

  uint64_t
  ToMS(Duration_t ms)
  {
    return ms.count();
  }

  /// get our uptime in ms
  Duration_t
  uptime()
  {
    return std::chrono::duration_cast<Duration_t>(
        std::chrono::steady_clock::now() - started_at_steady);
  }

  Duration_t
  time_now_ms()
  {
    auto t = uptime();
#ifdef TESTNET_SPEED
    t /= uint64_t{TESTNET_SPEED};
#endif
    return t + time_since_epoch<Duration_t, Clock_t>(started_at_system);
  }

  nlohmann::json
  to_json(const Duration_t& t)
  {
    return ToMS(t);
  }
}  // namespace llarp
