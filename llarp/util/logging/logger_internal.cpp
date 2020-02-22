#include <util/logging/logger_internal.hpp>

#include <date/date.h>

namespace llarp
{
  std::ostream&
  operator<<(std::ostream& out, const log_timestamp& ts)
  {
    std::chrono::milliseconds delta{ts.delta};
    auto h = std::chrono::duration_cast< std::chrono::hours >(delta);
    delta -= h;
    auto m = std::chrono::duration_cast< std::chrono::minutes >(delta);
    delta -= m;
    auto s = std::chrono::duration_cast< std::chrono::seconds >(delta);
    delta -= s;
    auto ms = delta;
    std::chrono::time_point< std::chrono::system_clock,
                             std::chrono::milliseconds >
        now{std::chrono::milliseconds{ts.now}};
    date::operator<<(out, now) << " UTC [+";
    auto old_fill = out.fill('0');
    if(h > 0h)
    {
      out << h.count() << 'h';
      out.width(2);  // 0-fill minutes if we have hours
    }
    if(h > 0h || m > 0min)
    {
      out << m.count() << 'm';
      out.width(2);  // 0-fill seconds if we have minutes
    }
    out << s.count() << '.';
    out.width(3);
    out << ms.count();
    out.fill(old_fill);
    return out << "s]";
  }
}  // namespace llarp
