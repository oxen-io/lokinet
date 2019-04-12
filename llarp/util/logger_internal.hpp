#ifndef LLARP_UTIL_LOGGER_INTERNAL_HPP
#define LLARP_UTIL_LOGGER_INTERNAL_HPP

#include <util/time.hpp>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <thread>

namespace llarp
{
  /** internal */
  template < typename TArg >
  void
  LogAppend(std::stringstream& ss, TArg&& arg) noexcept
  {
    ss << std::forward< TArg >(arg);
  }
  /** internal */
  template < typename TArg, typename... TArgs >
  void
  LogAppend(std::stringstream& ss, TArg&& arg, TArgs&&... args) noexcept
  {
    LogAppend(ss, std::forward< TArg >(arg));
    LogAppend(ss, std::forward< TArgs >(args)...);
  }

  static inline std::string
  thread_id_string()
  {
    auto tid = std::this_thread::get_id();
    std::hash< std::thread::id > h;
    uint16_t id = h(tid) % 1000;
#if defined(ANDROID) || defined(RPI)
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

    log_timestamp(const char* fmt = "%c %Z") : format(fmt)
    {
    }

    friend std::ostream&
    operator<<(std::ostream& out, const log_timestamp& ts)
    {
#if defined(ANDROID) || defined(RPI)
      (void)ts;
      return out << time_now_ms();
#else
      auto now = llarp::Clock_t::to_time_t(llarp::Clock_t::now());
      return out << std::put_time(std::localtime(&now), ts.format);
#endif
    }
  };

}  // namespace llarp

#endif
