#include <util/logging/logger.hpp>
#include <util/logging/logger.h>
#include <util/logging/ostream_logger.hpp>
#include <util/logging/logger_syslog.hpp>
#include <util/logging/file_logger.hpp>
#include <util/logging/json_logger.hpp>
#if defined(_WIN32)
#include <util/logging/win32_logger.hpp>
#endif
#if defined(ANDROID)
#include <util/logging/android_logger.hpp>
#endif

#include <util/str.hpp>

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
    if (str == "unknown") return LogType::Unknown;
    else if (str == "file") return LogType::File;
    else if (str == "json") return LogType::Json;
    else if (str == "syslog") return LogType::Syslog;

    return LogType::Unknown;
  }

  LogContext::LogContext()
      : logStream(std::make_unique<Stream_t>(_LOGSTREAM_INIT)), started(llarp::time_now_ms())
  {
  }

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
  {
  }

  log_timestamp::log_timestamp(const char* fmt)
      : format(fmt)
      , now(llarp::time_now_ms())
      , delta(llarp::time_now_ms() - LogContext::Instance().started)
  {
  }

  void
  SetLogLevel(LogLevel lvl)
  {
    LogContext::Instance().curLevel = lvl;
    if (lvl == eLogDebug)
    {
      LogContext::Instance().runtimeLevel = lvl;
    }
  }

  void
  LogContext::ImmediateFlush()
  {
    logStream->ImmediateFlush();
  }

  void
  LogContext::Initialize(LogLevel level,
                         LogType type,
                         const std::string& file,
                         const std::string& nickname,
                         std::shared_ptr<thread::ThreadPool> threadpool)
  {
    SetLogLevel(level);
    nodeName = nickname;

    FILE* logfile = nullptr;
    if (file == "stdout")
    {
      logfile = stdout;
    }
    else
    {
      logfile = ::fopen(file.c_str(), "a");
      if (not logfile)
      {
        throw std::runtime_error(stringify(
            "could not open logfile ", file, ", errno: ", strerror(errno)));
      }
    }

    switch (type)
    {
      case LogType::Unknown:
        throw std::invalid_argument("Cannot use LogType::Unknown");

      case LogType::File:
        if (logfile != stdout)
        {
          LogInfo("Switching logger to file ", file);
          std::cout << std::flush;

          LogContext::Instance().logStream =
              std::make_unique< FileLogStream >(threadpool, logfile, 100ms, true);
        }
        else
        {
          LogInfo("Logger remains stdout");
        }

        break;
      case LogType::Json:
        LogInfo("Switching logger to JSON with file: ", file);
        std::cout << std::flush;

        LogContext::Instance().logStream = std::make_unique< JSONLogStream >(
            threadpool, logfile, 100ms, logfile != stdout);
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
        LogContext::Instance().logStream = std::make_unique< SysLogStream >();
#endif
        break;

    }
  }

}  // namespace llarp

extern "C"
{
  void
  cSetLogLevel(LogLevel lvl)
  {
    llarp::SetLogLevel((llarp::LogLevel)lvl);
  }

  void
  cSetLogNodeName(const char* name)
  {
    llarp::LogContext::Instance().nodeName = name;
  }
}
