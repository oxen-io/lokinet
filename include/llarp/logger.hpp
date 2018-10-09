#ifndef LLARP_LOGGER_HPP
#define LLARP_LOGGER_HPP
#include <llarp/time.hpp>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <llarp/threading.hpp>
#include <sstream>
#include <string>
#ifdef _WIN32
#define VC_EXTRALEAN
#include <windows.h>
#endif
#ifdef ANDROID
#include <android/log.h>
#endif

namespace llarp
{
  // probably will need to move out of llarp namespace for c api
  enum LogLevel
  {
    eLogDebug,
    eLogInfo,
    eLogWarn,
    eLogError
  };

  struct Logger
  {
    std::string nodeName;
    LogLevel minlevel = eLogInfo;
    std::ostream& out;
    llarp::util::Mutex access;
    Logger() : Logger(std::cout, "unnamed")
    {
#ifdef _WIN32
      DWORD mode_flags;
      HANDLE fd1 = GetStdHandle(STD_OUTPUT_HANDLE);
      GetConsoleMode(fd1, &mode_flags);
      // since release SDKs don't have ANSI escape support yet
      mode_flags |= 0x0004;
      SetConsoleMode(fd1, mode_flags);
#endif
    }

    Logger(std::ostream& o, const std::string& name) : nodeName(name), out(o)
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
    return std::to_string(id);
  }

  struct log_timestamp
  {
    std::string format = "%c %Z";
    friend std::ostream&
    operator<<(std::ostream& out, const log_timestamp& ts)
    {
      auto now = llarp::Clock_t::to_time_t(llarp::Clock_t::now());
      return out << std::put_time(std::localtime(&now), ts.format.c_str());
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
    switch(lvl)
    {
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
#endif
    std::string tag = fname;
    ss << _glog.nodeName << " (" << thread_id_string() << ") "
       << log_timestamp() << " " << tag << ":" << lineno;
    ss << "\t";
    LogAppend(ss, std::forward< TArgs >(args)...);
#ifndef ANDROID
    ss << (char)27 << "[0;0m";
    _glog.out << ss.str() << std::endl;
#else
    {
      std::unique_lock< std::mutex > lock(_glog.access);
      tag = "LOKINET|" + tag;
      __android_log_write(ANDROID_LOG_INFO, tag.c_str(), ss.str().c_str());
      //__android_log_write(ANDROID_LOG_INFO, "LOKINET", ss.str().c_str());
      llarp::util::Lock lock(_glog.access);
      _glog.out << ss.str() << std::endl;
    }
#endif
#ifdef SHADOW_TESTNET
    _glog.out << "\n" << std::flush;
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
