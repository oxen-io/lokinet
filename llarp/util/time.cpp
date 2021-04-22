#include "time.hpp"
#include <chrono>

namespace llarp
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

  std::ostream&
  operator<<(std::ostream& out, const Duration_t& t)
  {
    std::chrono::milliseconds amount{ToMS(t)};
    auto h = std::chrono::duration_cast<std::chrono::hours>(amount);
    amount -= h;
    auto m = std::chrono::duration_cast<std::chrono::minutes>(amount);
    amount -= m;
    auto s = std::chrono::duration_cast<std::chrono::seconds>(amount);
    amount -= s;
    auto ms = amount;
    auto old_fill = out.fill('0');
    if (h > 0h)
    {
      out << h.count() << 'h';
      out.width(2);  // 0-fill minutes if we have hours
    }
    if (h > 0h || m > 0min)
    {
      out << m.count() << 'm';
      out.width(2);  // 0-fill seconds if we have minutes
    }
    out << s.count() << '.';
    out.width(3);
    out << ms.count();
    out.fill(old_fill);
    return out << "s";
  }
}  // namespace llarp
