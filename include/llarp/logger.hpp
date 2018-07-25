#ifndef LLARP_LOGGER_HPP
#define LLARP_LOGGER_HPP

#include <llarp/time.h>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <llarp/threading.hpp>
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
    eLogError
  };

  struct Logger
  {
    LogLevel minlevel = eLogInfo;
    std::ostream& out;
    std::mutex access;
    Logger() : Logger(std::cout)
    {
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

  /** internal */
  template < typename... TArgs >
  void
  _Log(LogLevel lvl, const char* fname, TArgs&&... args) noexcept
  {
    if(_glog.minlevel > lvl)
      return;

    std::stringstream ss;
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
    std::string tag = fname;
    ss << llarp_time_now_ms() << " " << tag;
    ss << "\t";
    LogAppend(ss, std::forward< TArgs >(args)...);
    ss << (char)27 << "[0;0m";
    {
      std::unique_lock< std::mutex > lock(_glog.access);
      _glog.out << ss.str() << std::endl;
#ifdef SHADOW_TESTNET
      _glog.out << "\n" << std::flush;
#endif
    }
  }
}  // namespace llarp

#define LogDebug(x, ...) _Log(llarp::eLogDebug, __FILE__, x, ##__VA_ARGS__)
#define LogInfo(x, ...) _Log(llarp::eLogInfo, __FILE__, x, ##__VA_ARGS__)
#define LogWarn(x, ...) _Log(llarp::eLogWarn, __FILE__, x, ##__VA_ARGS__)
#define LogError(x, ...) _Log(llarp::eLogError, __FILE__, x, ##__VA_ARGS__)

#endif
