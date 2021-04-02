#include "logger.hpp"
#include "ostream_logger.hpp"
#include "logger_syslog.hpp"
#include "file_logger.hpp"
#include "json_logger.hpp"
#if defined(_WIN32)
#include "win32_logger.hpp"
#endif
#if defined(ANDROID)
#include "android_logger.hpp"
#endif

#include <llarp/util/str.hpp>

#include <stdexcept>

namespace llarp
{
#if defined(_WIN32)
  using Stream_t = Win32LogStream;
#define _LOGSTREAM_INIT std::cout
#else
#if defined(ANDROID)
  using Stream_t = AndroidLogStream;
#define _LOGSTREAM_INIT
#else
  using Stream_t = OStreamLogStream;
#define _LOGSTREAM_INIT true, std::cout
#endif
#endif

  LogType
  LogTypeFromString(const std::string& str)
  {
    if (str == "unknown")
      return LogType::Unknown;
    else if (str == "file")
      return LogType::File;
    else if (str == "json")
      return LogType::Json;
    else if (str == "syslog")
      return LogType::Syslog;

    return LogType::Unknown;
  }

  LogContext::LogContext() : logStream{std::make_unique<Stream_t>(_LOGSTREAM_INIT)}
  {}

  LogContext&
  LogContext::Instance()
  {
    static LogContext ctx;
    return ctx;
  }

  void
  LogContext::DropToRuntimeLevel()
  {
    curLevel = runtimeLevel;
  }

  void
  LogContext::RevertRuntimeLevel()
  {
    curLevel = startupLevel;
  }

  log_timestamp::log_timestamp() : log_timestamp("%c %Z")
  {}

  log_timestamp::log_timestamp(const char* fmt)
      : format{fmt}, now{llarp::time_now_ms()}, delta{llarp::uptime()}
  {}

  void
  SetLogLevel(LogLevel lvl)
  {
    LogContext::Instance().curLevel = lvl;
    LogContext::Instance().runtimeLevel = lvl;
  }

  LogLevel
  GetLogLevel()
  {
    return LogContext::Instance().curLevel;
  }

  void
  LogContext::ImmediateFlush()
  {
    logStream->ImmediateFlush();
  }

  void
  LogContext::Initialize(
      LogLevel level,
      LogType type,
      const std::string& file,
      const std::string& nickname,
      std::function<void(IOFunc_t)> io)
  {
    SetLogLevel(level);
    if (level == eLogTrace)
      LogTrace("Set log level to trace.");

    nodeName = nickname;

    FILE* logfile = nullptr;
    if (file == "stdout" or file == "-" or file.empty())
    {
      logfile = stdout;
    }
    else
    {
      logfile = ::fopen(file.c_str(), "a");
      if (not logfile)
      {
        throw std::runtime_error(
            stringify("could not open logfile ", file, ", errno: ", strerror(errno)));
      }
    }

    switch (type)
    {
      case LogType::Unknown:
        // tolerate as fallback to LogType::File

      case LogType::File:
        if (logfile != stdout)
        {
          LogInfo("Switching logger to file ", file);
          std::cout << std::flush;

          LogContext::Instance().logStream =
              std::make_unique<FileLogStream>(io, logfile, 100ms, true);
        }
        else
        {
          LogInfo("Logger remains stdout");
        }

        break;
      case LogType::Json:
        LogInfo("Switching logger to JSON with file: ", file);
        std::cout << std::flush;

        LogContext::Instance().logStream =
            std::make_unique<JSONLogStream>(io, logfile, 100ms, logfile != stdout);
        break;
      case LogType::Syslog:
        if (logfile)
        {
          // TODO: this logic should be handled in Config
          // TODO: this won't even work because of default value for 'file' (== "stdout")
          ::fclose(logfile);
          throw std::invalid_argument("Cannot mix log type=syslog and file=*");
        }
#if defined(_WIN32)
        throw std::runtime_error("syslog not supported on win32");
#else
        LogInfo("Switching logger to syslog");
        std::cout << std::flush;
        LogContext::Instance().logStream = std::make_unique<SysLogStream>();
#endif
        break;
    }
  }

  LogSilencer::LogSilencer() : LogSilencer(LogContext::Instance())
  {}

  LogSilencer::LogSilencer(LogContext& ctx) : parent(ctx), stream(std::move(ctx.logStream))
  {}

  LogSilencer::~LogSilencer()
  {
    parent.logStream = std::move(stream);
  }

}  // namespace llarp
