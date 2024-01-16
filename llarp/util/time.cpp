#include "time.hpp"

#include "types.hpp"

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

  rc_time
  time_point_now()
  {
    return std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());
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

  static auto
  extract_h_m_s_ms(const Duration_t& dur)
  {
    return std::make_tuple(
        std::chrono::duration_cast<std::chrono::hours>(dur).count(),
        (std::chrono::duration_cast<std::chrono::minutes>(dur) % 1h).count(),
        (std::chrono::duration_cast<std::chrono::seconds>(dur) % 1min).count(),
        (std::chrono::duration_cast<std::chrono::milliseconds>(dur) % 1s).count());
  }

  std::string
  short_time_from_now(const TimePoint_t& t, const Duration_t& now_threshold)
  {
    auto delta = std::chrono::duration_cast<Duration_t>(llarp::TimePoint_t::clock::now() - t);
    bool future = delta < 0s;
    if (future)
      delta = -delta;

    auto [hours, mins, secs, ms] = extract_h_m_s_ms(delta);

    using namespace fmt::literals;
    return fmt::format(
        delta < now_threshold ? "now"
            : delta < 10s     ? "{in}{secs:d}.{ms:03d}s{ago}"
            : delta < 1h      ? "{in}{mins:d}m{secs:02d}s{ago}"
                              : "{in}{hours:d}h{mins:02d}m{ago}",
        "in"_a = future ? "in " : "",
        "ago"_a = future ? "" : " ago",
        "hours"_a = hours,
        "mins"_a = mins,
        "secs"_a = secs,
        "ms"_a = ms);
  }

  std::string
  ToString(Duration_t delta)
  {
    bool neg = delta < 0s;
    if (neg)
      delta = -delta;

    auto [hours, mins, secs, ms] = extract_h_m_s_ms(delta);

    using namespace fmt::literals;
    return fmt::format(
        delta < 1min     ? "{neg}{secs:d}.{ms:03d}s"
            : delta < 1h ? "{neg}{mins:d}m{secs:02d}.{ms:03d}s"
                         : "{neg}{hours:d}h{mins:02d}m{secs:02d}.{ms:03d}s",
        "neg"_a = neg ? "-" : "",
        "hours"_a = hours,
        "mins"_a = mins,
        "secs"_a = secs,
        "ms"_a = ms);
  }
}  // namespace llarp
