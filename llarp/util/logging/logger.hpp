#pragma once

#include <memory>
#include <llarp/util/str.hpp>
#include <llarp/util/time.hpp>
#include "logstream.hpp"
#include "logger_internal.hpp"
#include "source_location.hpp"

namespace llarp
{
  enum class LogType
  {
    Unknown = 0,
    File,
    Syslog,
  };
  LogType
  LogTypeFromString(const std::string&);

  struct LogContext
  {
    using IOFunc_t = std::function<void(void)>;

    LogContext();
    LogLevel curLevel = eLogInfo;
    LogLevel startupLevel = eLogInfo;
    LogLevel runtimeLevel = eLogWarn;
    ILogStream_ptr logStream;
    std::string nodeName = "lokinet";

    static LogContext&
    Instance();

    void
    DropToRuntimeLevel();

    void
    RevertRuntimeLevel();

    /// A blocking call that will not return until any existing log functions have flushed.
    /// Should only be called in rare circumstances, such as when the program is about to exit.
    void
    ImmediateFlush();

    /// Initialize the logging system.
    ///
    /// @param level is the new log level (below which log statements will be ignored)
    /// @param type is the type of logger to set up
    /// @param file is the file to log to (relevant for types File and Json)
    /// @param nickname is a tag to add to each log statement
    /// @param io is a callable that queues work that does io, async
    void
    Initialize(
        LogLevel level,
        LogType type,
        const std::string& file,
        const std::string& nickname,
        std::function<void(IOFunc_t)> io);
  };

  /// RAII type to turn logging off
  /// logging is suppressed as long as the silencer is in scope
  struct LogSilencer
  {
    LogSilencer();
    ~LogSilencer();
    explicit LogSilencer(LogContext& ctx);

   private:
    LogContext& parent;
    ILogStream_ptr stream;
  };

  void
  SetLogLevel(LogLevel lvl);

  LogLevel
  GetLogLevel();

  /** internal */
  template <typename... TArgs>
  inline static void
  _log(LogLevel lvl, const slns::source_location& location, TArgs&&... args) noexcept
  {
    auto& log = LogContext::Instance();
    if (log.curLevel > lvl || log.logStream == nullptr)
      return;
    std::ostringstream ss;
    if constexpr (sizeof...(args) > 0)
      LogAppend(ss, std::forward<TArgs>(args)...);
    log.logStream->AppendLog(
        lvl,
        strip_prefix(location.file_name(), SOURCE_ROOT),
        location.line(),
        log.nodeName,
        ss.str());
  }

  template <typename... T>
  struct LogTrace
  {
    LogTrace(
        [[maybe_unused]] T... args,
        [[maybe_unused]] const slns::source_location& location = slns::source_location::current())
    {
#ifndef NDEBUG
      _log(eLogTrace, location, std::forward<T>(args)...);
#endif
    }
  };

  template <typename... T>
  LogTrace(T&&...) -> LogTrace<T...>;

  template <typename... T>
  struct LogDebug
  {
    LogDebug(
        [[maybe_unused]] T... args,
        [[maybe_unused]] const slns::source_location& location = slns::source_location::current())
    {
#ifndef NDEBUG
      _log(eLogDebug, location, std::forward<T>(args)...);
#endif
    }
  };

  template <typename... T>
  LogDebug(T&&...) -> LogDebug<T...>;

  template <typename... T>
  struct LogInfo
  {
    LogInfo(T... args, const slns::source_location& location = slns::source_location::current())
    {
      _log(eLogInfo, location, std::forward<T>(args)...);
    }
  };

  template <typename... T>
  LogInfo(T&&...) -> LogInfo<T...>;

  template <typename... T>
  struct LogWarn
  {
    LogWarn(T... args, const slns::source_location& location = slns::source_location::current())
    {
      _log(eLogWarn, location, std::forward<T>(args)...);
    }
  };

  template <typename... T>
  LogWarn(T&&...) -> LogWarn<T...>;

  template <typename... T>
  struct LogError
  {
    LogError(T... args, const slns::source_location& location = slns::source_location::current())
    {
      _log(eLogError, location, std::forward<T>(args)...);
    }
  };

  template <typename... T>
  LogError(T&&...) -> LogError<T...>;

}  // namespace llarp
