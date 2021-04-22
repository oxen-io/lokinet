#include "logger_internal.hpp"

#include <date/date.h>

namespace llarp
{
  std::ostream&
  operator<<(std::ostream& out, const log_timestamp& ts)
  {
    std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> now{
        std::chrono::milliseconds{ts.now}};
    return date::operator<<(out, now) << " GMT [+" << ts.delta << "]";
  }
}  // namespace llarp
