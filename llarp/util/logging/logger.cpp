#include <util/logging/logger.hpp>
#include <util/logging/logger.h>
#include <util/logging/ostream_logger.hpp>
#if defined(_WIN32)
#include <util/logging/win32_logger.hpp>
#endif
#if defined(ANDROID)
#include <util/logging/android_logger.hpp>
#endif

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

  LogContext::LogContext()
      : logStream(std::make_unique< Stream_t >(_LOGSTREAM_INIT))
      , started(llarp::time_now_ms())
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
