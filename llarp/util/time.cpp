#include "time.hpp"
#include <chrono>
#include <iomanip>

namespace llarp
{
  namespace
  {
    const static auto started_at_steady = std::chrono::steady_clock::now();
    const static auto started_at_system = DateClock_t::now();
  }  // namespace

  uint64_t
  ToMS(Duration_t ms)
  {
    return ms.count();
  }

  uint64_t
  to_unix_stamp(const TimePoint_t& t)
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count();
  }

  /// get our uptime in ms
  Duration_t
  uptime()
  {
    return std::chrono::duration_cast<Duration_t>(
        std::chrono::steady_clock::now() - started_at_steady);
  }

  TimePoint_t
  started_at()
  {
    return started_at_system;
  }

  nlohmann::json
  to_json(const Duration_t& t)
  {
    return ToMS(t);
  }
}  // namespace llarp
