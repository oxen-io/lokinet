#ifndef LLARP_TIME_HPP
#define LLARP_TIME_HPP

#include <util/types.hpp>
#include <nlohmann/json.hpp>

using namespace std::chrono_literals;

namespace llarp
{
  /// get time right now as milliseconds, this is monotonic
  llarp_time_t
  time_now_ms();

  std::ostream &
  operator<<(std::ostream &out, const llarp_time_t &t);

  nlohmann::json
  to_json(const llarp_time_t &t);

}  // namespace llarp

#endif
