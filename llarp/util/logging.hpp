#pragma once

// Header for making actual log statements such as llarp::log::Info and so on work.

#include <string>
#include <string_view>

#include <oxen/log.hpp>
#include "oxen/log/internal.hpp"

namespace llarp
{
  namespace log = oxen::log;
}

// Not ready to pollute these deprecation warnings everywhere yet
#if 0
#define LOKINET_LOG_DEPRECATED(Meth) \
  [[deprecated("Use formatted log::" #Meth "(cat, fmt, args...) instead")]]
#else
#define LOKINET_LOG_DEPRECATED(Meth)
#endif

// Deprecated loggers (in the top-level llarp namespace):
namespace llarp
{
  namespace log_detail
  {
    inline log::CategoryLogger legacy_logger = log::Cat("");

    template <typename>
    struct concat_args_fmt_impl;
    template <size_t... I>
    struct concat_args_fmt_impl<std::integer_sequence<size_t, I...>>
    {
      constexpr static std::array<char, sizeof...(I)> format{(I % 2 == 0 ? '{' : '}')...};
    };
    template <size_t N>
    constexpr std::string_view
    concat_args_fmt()
    {
      return std::string_view{
          concat_args_fmt_impl<std::make_index_sequence<2 * N>>::format.data(), 2 * N};
    }

    template <log::Level level, typename... T>
    struct LegacyLeveledLogger : log::detail::LeveledLogger<level, T...>
    {
      LegacyLeveledLogger(
          T&&... args, const slns::source_location& location = slns::source_location::current())
          : log::detail::LeveledLogger<level, T...>::LeveledLogger{
              legacy_logger, concat_args_fmt<sizeof...(T)>(), std::forward<T>(args)..., location}
      {}
    };
  }  // namespace log_detail

  template <typename... T>
  struct LOKINET_LOG_DEPRECATED(Trace) LogTrace
      : log_detail::LegacyLeveledLogger<log::Level::trace, T...>
  {
    using log_detail::LegacyLeveledLogger<log::Level::trace, T...>::LegacyLeveledLogger;
  };
  template <typename... T>
  struct LOKINET_LOG_DEPRECATED(Debug) LogDebug
      : log_detail::LegacyLeveledLogger<log::Level::debug, T...>
  {
    using log_detail::LegacyLeveledLogger<log::Level::debug, T...>::LegacyLeveledLogger;
  };
  template <typename... T>
  struct LOKINET_LOG_DEPRECATED(Info) LogInfo
      : log_detail::LegacyLeveledLogger<log::Level::info, T...>
  {
    using log_detail::LegacyLeveledLogger<log::Level::info, T...>::LegacyLeveledLogger;
  };
  template <typename... T>
  struct LOKINET_LOG_DEPRECATED(Warning) LogWarn
      : log_detail::LegacyLeveledLogger<log::Level::warn, T...>
  {
    using log_detail::LegacyLeveledLogger<log::Level::warn, T...>::LegacyLeveledLogger;
  };
  template <typename... T>
  struct LOKINET_LOG_DEPRECATED(Error) LogError
      : log_detail::LegacyLeveledLogger<log::Level::err, T...>
  {
    using log_detail::LegacyLeveledLogger<log::Level::err, T...>::LegacyLeveledLogger;
  };

  template <typename... T>
  LogTrace(T&&...) -> LogTrace<T...>;

  template <typename... T>
  LogDebug(T&&...) -> LogDebug<T...>;

  template <typename... T>
  LogInfo(T&&...) -> LogInfo<T...>;

  template <typename... T>
  LogWarn(T&&...) -> LogWarn<T...>;

  template <typename... T>
  LogError(T&&...) -> LogError<T...>;

}  // namespace llarp
