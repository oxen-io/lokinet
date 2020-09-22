#include <util/time.hpp>
#include <chrono>
#include <util/logging/logger.hpp>

namespace llarp
{
  using Clock_t = std::chrono::system_clock;

  template <typename Res, typename Clock>
  static llarp_time_t
  time_since_epoch()
  {
    return std::chrono::duration_cast<Res>(Clock::now().time_since_epoch());
  }

  const static llarp_time_t started_at_system =
      time_since_epoch<std::chrono::milliseconds, Clock_t>();

  const static llarp_time_t started_at_steady =
      time_since_epoch<std::chrono::milliseconds, std::chrono::steady_clock>();
  /// get our uptime in ms
  static llarp_time_t
  time_since_started()
  {
    return time_since_epoch<std::chrono::milliseconds, std::chrono::steady_clock>()
        - started_at_steady;
  }

  llarp_time_t
  time_now_ms()
  {
    static llarp_time_t lastTime = 0s;
    auto t = time_since_started();
#ifdef TESTNET_SPEED
    t /= uint64_t(TESTNET_SPEED);
#endif
    t += started_at_system;

    if (t <= lastTime)
    {
      return lastTime;
    }
    if (lastTime == 0s)
    {
      lastTime = t;
    }
    const auto dlt = t - lastTime;
    if (dlt > 5s)
    {
      // big timeskip
      t = lastTime;
      lastTime = 0s;
    }
    else
    {
      lastTime = t;
    }
    return t;
  }

  nlohmann::json
  to_json(const llarp_time_t& t)
  {
    return t.count();
  }

  std::ostream&
  operator<<(std::ostream& out, const llarp_time_t& t)
  {
    std::chrono::milliseconds amount = t;
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
