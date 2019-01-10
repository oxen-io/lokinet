#ifndef LLARP_LOGGER_HPP
#define LLARP_LOGGER_HPP

#include <util/threading.hpp>
#include <util/time.hpp>

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

namespace llarp
{
  // probably will need to move out of llarp namespace for c api
  enum LogLevel
  {
    eLogDebug,
    eLogInfo,
    eLogWarn,
    eLogError,
    eLogNone
  };

  struct Logger
  {
    std::string nodeName;
    LogLevel minlevel = eLogInfo;
    std::ostream& out;

    std::function< void(const std::string&) > customLog;

    llarp::util::Mutex access;
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
  };

  extern Logger _glog;

  void
  SetLogLevel(LogLevel lvl);

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

  /** internal */
  template < typename... TArgs >
  void
  _Log(LogLevel lvl, const char* fname, int lineno, TArgs&&... args) noexcept
  {
    if(_glog.minlevel > lvl)
      return;

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
}  // namespace llarp

#define LogDebug(x, ...) \
  _Log(llarp::eLogDebug, LOG_TAG, __LINE__, x, ##__VA_ARGS__)
#define LogInfo(x, ...) \
  _Log(llarp::eLogInfo, LOG_TAG, __LINE__, x, ##__VA_ARGS__)
#define LogWarn(x, ...) \
  _Log(llarp::eLogWarn, LOG_TAG, __LINE__, x, ##__VA_ARGS__)
#define LogError(x, ...) \
  _Log(llarp::eLogError, LOG_TAG, __LINE__, x, ##__VA_ARGS__)
#define LogDebugTag(tag, x, ...) \
  _Log(llarp::eLogDebug, tag, __LINE__, x, ##__VA_ARGS__)
#define LogInfoTag(tag, x, ...) \
  _Log(llarp::eLogInfo, tag, __LINE__, x, ##__VA_ARGS__)
#define LogWarnTag(tag, x, ...) \
  _Log(llarp::eLogWarn, tag, __LINE__, x, ##__VA_ARGS__)
#define LogErrorTag(tag, x, ...) \
  _Log(llarp::eLogError, tag, __LINE__, x, ##__VA_ARGS__)

#ifndef LOG_TAG
#define LOG_TAG "default"
#endif

#endif
