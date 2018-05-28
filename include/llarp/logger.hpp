#ifndef LLARP_LOGGER_HPP
#define LLARP_LOGGER_HPP

#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace llarp
{
  enum LogLevel
  {
    eLogDebug,
    eLogInfo,
    eLogWarn,
    eLogError
  };

  struct Logger
  {
    LogLevel minlevel = eLogDebug;
    std::ostream& out = std::cout;
  };

  extern Logger _glog;

  void
  SetLogLevel(LogLevel lvl);

  /** internal */
  template < typename TArg >
  void
  LogAppend(std::stringstream& ss, TArg&& arg)
  {
    ss << std::forward< TArg >(arg);
  }
  /** internal */
  template < typename TArg, typename... TArgs >
  void
  LogAppend(std::stringstream& ss, TArg&& arg, TArgs&&... args)
  {
    LogAppend(ss, std::forward< TArg >(arg));
    LogAppend(ss, std::forward< TArgs >(args)...);
  }

  /** internal */
  template < typename... TArgs >
  void
  Log(LogLevel lvl, const char* tag, TArgs&&... args)
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
    auto t   = std::time(nullptr);
    auto now = std::localtime(&t);
    ss << std::put_time(now, "%F %T") << " " << tag << "\t";
    LogAppend(ss, std::forward< TArgs >(args)...);
    ss << (char)27 << "[0;0m";
    _glog.out << ss.str() << std::endl;
  }

  template < typename... TArgs >
  void
  Debug(const char* tag, TArgs&&... args)
  {
    Log(eLogDebug, tag, std::forward< TArgs >(args)...);
  }

  template < typename... TArgs >
  void
  Info(const char* tag, TArgs&&... args)
  {
    Log(eLogInfo, tag, std::forward< TArgs >(args)...);
  }

  template < typename... TArgs >
  void
  Warn(const char* tag, TArgs&&... args)
  {
    Log(eLogWarn, tag, std::forward< TArgs >(args)...);
  }

  template < typename... TArgs >
  void
  Error(const char* tag, TArgs&&... args)
  {
    Log(eLogError, tag, std::forward< TArgs >(args)...);
  }
}

#endif
