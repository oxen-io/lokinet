#ifndef LLARP_UTIL_LOGGER_INTERNAL_HPP
#define LLARP_UTIL_LOGGER_INTERNAL_HPP

#include <util/time.hpp>

#include <ctime>
#include <sstream>
#include <util/thread/threading.hpp>

namespace llarp
{
  /** internal, recursion terminator */
  constexpr void
  LogAppend(std::stringstream&) noexcept
  {}
  /** internal */
  template <typename TArg, typename... TArgs>
  void
  LogAppend(std::stringstream& ss, TArg&& arg, TArgs&&... args) noexcept
  {
    ss << std::forward<TArg>(arg);
    LogAppend(ss, std::forward<TArgs>(args)...);
  }

  inline std::string
  thread_id_string()
  {
    auto tid = std::this_thread::get_id();
    std::hash<std::thread::id> h;
    uint16_t id = h(tid) % 1000;
#if defined(ANDROID)
    char buff[8] = {0};
    snprintf(buff, sizeof(buff), "%u", id);
    return buff;
#else
    return std::to_string(id);
#endif
  }

  struct log_timestamp
  {
    const char* format;
    const llarp_time_t now;
    const llarp_time_t delta;

    log_timestamp();

    explicit log_timestamp(const char* fmt);
  };

  std::ostream&
  operator<<(std::ostream& out, const log_timestamp& ts);

}  // namespace llarp

#endif
