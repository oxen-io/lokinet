#pragma once

#include <llarp/util/time.hpp>

#include <ctime>
#include <sstream>
#include <llarp/util/thread/threading.hpp>
#include <type_traits>

namespace llarp
{
  /** internal */

  // true if T is the same as any of V...
  template <typename T, typename... V>
  constexpr bool is_same_any_v = (std::is_same_v<T, V> || ...);

  template <typename TArg, typename... TArgs>
  void
  LogAppend(std::ostringstream& ss, TArg&& arg, TArgs&&... args) noexcept
  {
    // If you are logging a char/unsigned char/uint8_t then promote it to an integer so that we
    // print numeric values rather than std::ostream's default of printing it as a raw char.
    using PlainT = std::remove_reference_t<TArg>;
    if constexpr (is_same_any_v<PlainT, char, unsigned char, signed char, uint8_t>)
      ss << +std::forward<TArg>(arg);  // Promote to int
    else if constexpr (std::is_same_v<PlainT, std::byte>)
      ss << std::to_integer<int>(arg);
    else
      ss << std::forward<TArg>(arg);
    if constexpr (sizeof...(TArgs) > 0)
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
    const Duration_t now;
    const Duration_t delta;

    log_timestamp();

    explicit log_timestamp(const char* fmt);
  };

  std::ostream&
  operator<<(std::ostream& out, const log_timestamp& ts);

}  // namespace llarp
