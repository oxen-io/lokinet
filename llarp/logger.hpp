#ifndef LLARP_LOGGER_HPP
#define LLARP_LOGGER_HPP

#include <chrono>
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

  extern LogLevel loglevel;

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
    if(loglevel < lvl)
      return;

    std::stringstream ss;
    switch(lvl)
    {
      case eLogDebug:
        ss << "[DBG] ";
        break;
      case eLogInfo:
        ss << "[NFO] ";
        break;
      case eLogWarn:
        ss << "[WRN] ";
        break;
      case eLogError:
        ss << "[ERR] ";
        break;
    }
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    ss << std::chrono::duration_cast< std::chrono::milliseconds >(now).count()
       << " ";
    LogAppend(ss, std::forward< TArgs >(args)...);
    std::cerr << ss.str() << std::endl;
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
