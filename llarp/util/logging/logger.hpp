#ifndef LLARP_UTIL_LOGGER_HPP
#define LLARP_UTIL_LOGGER_HPP

#include <util/time.hpp>
#include <util/logging/logstream.hpp>
#include <util/logging/logger_internal.hpp>
/*
#ifdef _WIN32
#define VC_EXTRALEAN
#include <windows.h>
#endif
#ifdef ANDROID
#include <android/log.h>
#endif
#ifdef RPI
#include <cstdio>
#endif

#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <functional>
*/

namespace llarp
{
  /*
  struct Logger
  {
    std::string nodeName;
    LogLevel minlevel = eLogInfo;
    ILogStream_ptr m_stream;

    std::unique_ptr< std::ostream > _altOut;

#ifdef _WIN32
    bool isConsoleModern =
        true;  // qol fix so oldfag clients don't see ugly escapes
    HANDLE fd1 = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    short old_attrs;
#endif
    Logger() : Logger(std::cout)
    {
#ifdef _WIN32
      // Attempt to use ANSI escapes directly
      // if the modern console is active.
      DWORD mode_flags;

      GetConsoleMode(fd1, &mode_flags);
      // since release SDKs don't have ANSI escape support yet
      // we get all or nothing: if we can't get it, then we wouldn't
      // be able to get any of them individually
      mode_flags |= 0x0004 | 0x0008;
      BOOL t = SetConsoleMode(fd1, mode_flags);
      if(!t)
        this->isConsoleModern = false;  // fall back to setting colours manually
#endif
    }

    Logger(std::ostream& o) : out(o)
    {
    }

    /// open logger to file
    /// return false on failure
    /// return true on successful open
    bool
    OpenLogFile(const std::string& file);

    /// call once to use syslog based logging
    void
    UseSyslog();

    virtual void
    Print(LogLevel lvl, const std::string& msg);
  };
  */

  struct LogContext
  {
    LogContext();
    LogLevel curLevel     = eLogInfo;
    LogLevel startupLevel = eLogInfo;
    LogLevel runtimeLevel = eLogInfo;
    ILogStream_ptr logStream;
    std::string nodeName = "lokinet";

    const llarp_time_t started;

    static LogContext&
    Instance();

    void
    DropToRuntimeLevel();

    void
    RevertRuntimeLevel();
  };

  void
  SetLogLevel(LogLevel lvl);

  /** internal */
  template < typename... TArgs >
  inline static void
#ifndef LOKINET_HIVE
  _Log(LogLevel lvl, const char* fname, int lineno, TArgs&&... args) noexcept
#else
  _Log(LogLevel, const char*, int, TArgs&&...) noexcept
#endif
  {
/* nop out logging for hive mode for now */
#ifndef LOKINET_HIVE
    auto& log = LogContext::Instance();
    if(log.curLevel > lvl)
      return;
    std::stringstream ss;
    LogAppend(ss, std::forward< TArgs >(args)...);
    log.logStream->AppendLog(lvl, fname, lineno, log.nodeName, ss.str());
#endif
  }
  /*
    std::stringstream ss;
#ifdef ANDROID
    int loglev = -1;
    switch(lvl)
    {
      case eLogNone:
        break;
      case eLogDebug:
        ss << "[DBG] ";
        loglev = ANDROID_LOG_DEBUG;
        break;
      case eLogInfo:
        ss << "[NFO] ";
        loglev = ANDROID_LOG_INFO;
        break;
      case eLogWarn:
        ss << "[WRN] ";
        loglev = ANDROID_LOG_WARN;
        break;
      case eLogError:
        ss << "[ERR] ";
        loglev = ANDROID_LOG_ERROR;
        break;
    }
#else
#ifdef _WIN32
    if(_glog.isConsoleModern)
    {
#endif
      switch(lvl)
      {
        case eLogNone:
          break;
        case eLogDebug:
          ss << (char)27 << "[0m";
          ss << "[DBG] ";
          break;
        case eLogInfo:
          ss << (char)27 << "[1m";
          ss << "[NFO] ";
          break;
        case eLogWarn:
          ss << (char)27 << "[1;33m";
          ss << "[WRN] ";
          break;
        case eLogError:
          ss << (char)27 << "[1;31m";
          ss << "[ERR] ";
          break;
      }
#ifdef _WIN32
    }
    else  // legacy console
    {
      // these _should_ be low white on black
      GetConsoleScreenBufferInfo(_glog.fd1, &_glog.consoleInfo);
      _glog.old_attrs = _glog.consoleInfo.wAttributes;
      switch(lvl)
      {
        case eLogNone:
          break;
        case eLogDebug:
          SetConsoleTextAttribute(_glog.fd1,
                                  FOREGROUND_RED | FOREGROUND_GREEN
                                      | FOREGROUND_BLUE);  // low white on black
          ss << "[DBG] ";
          break;
        case eLogInfo:
          SetConsoleTextAttribute(
              _glog.fd1,
              FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN
                  | FOREGROUND_BLUE);  // high white on black
          ss << "[NFO] ";
          break;
        case eLogWarn:
          SetConsoleTextAttribute(_glog.fd1,
                                  FOREGROUND_RED | FOREGROUND_GREEN
                                      | FOREGROUND_INTENSITY);  // bright yellow
          ss << "[WRN] ";
          break;
        case eLogError:
          SetConsoleTextAttribute(
              _glog.fd1, FOREGROUND_RED | FOREGROUND_INTENSITY);  // bright red
          ss << "[ERR] ";
          break;
      }
    }
#endif
#endif
    std::string tag = fname;
    if(_glog.nodeName.size())
      ss << _glog.nodeName << " ";
    ss << "(" << thread_id_string() << ") " << log_timestamp() << " " << tag
       << ":" << lineno;
    ss << "\t";
    LogAppend(ss, std::forward< TArgs >(args)...);
#ifndef ANDROID
#ifdef _WIN32
    if(_glog.isConsoleModern)
    {
#endif
      ss << (char)27 << "[0;0m";
      _glog.out << ss.str() << std::endl;
#ifdef TESTNET
      _glog.out << std::flush;
#endif
#ifdef _WIN32
    }
    else
    {
      _glog.out << ss.str() << std::endl;
      SetConsoleTextAttribute(
          _glog.fd1, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }

#endif
#else
    {
      tag = "LOKINET|" + tag;
      __android_log_write(loglev, tag.c_str(), ss.str().c_str());
    }
#endif
  }
  */
}  // namespace llarp

#define LogTrace(...) _Log(llarp::eLogTrace, LOG_TAG, __LINE__, __VA_ARGS__)
#define LogDebug(...) _Log(llarp::eLogDebug, LOG_TAG, __LINE__, __VA_ARGS__)
#define LogInfo(...) _Log(llarp::eLogInfo, LOG_TAG, __LINE__, __VA_ARGS__)
#define LogWarn(...) _Log(llarp::eLogWarn, LOG_TAG, __LINE__, __VA_ARGS__)
#define LogError(...) _Log(llarp::eLogError, LOG_TAG, __LINE__, __VA_ARGS__)

#define LogTraceTag(tag, ...) _Log(llarp::eLogTrace, tag, __LINE__, __VA_ARGS__)
#define LogDebugTag(tag, ...) _Log(llarp::eLogDebug, tag, __LINE__, __VA_ARGS__)
#define LogInfoTag(tag, ...) _Log(llarp::eLogInfo, tag, __LINE__, __VA_ARGS__)
#define LogWarnTag(tag, ...) _Log(llarp::eLogWarn, tag, __LINE__, __VA_ARGS__)
#define LogErrorTag(tag, ...) _Log(llarp::eLogError, tag, __LINE__, __VA_ARGS__)

#define LogTraceExplicit(tag, line, ...) \
  _Log(llarp::eLogTrace, tag, line, __VA_ARGS__)
#define LogDebugExplicit(tag, line, ...) \
  _Log(llarp::eLogDebug, tag, line, __VA_ARGS__)
#define LogInfoExplicit(tag, line, ...) \
  _Log(llarp::eLogInfo, tag, line __VA_ARGS__)
#define LogWarnExplicit(tag, line, ...) \
  _Log(llarp::eLogWarn, tag, line, __VA_ARGS__)
#define LogErrorExplicit(tag, line, ...) \
  _Log(llarp::eLogError, tag, line, __VA_ARGS__)

#ifndef LOG_TAG
#define LOG_TAG "default"
#endif

#endif
