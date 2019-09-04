#include <util/logging/loglevel.hpp>

namespace llarp
{
  std::string
  LogLevelToString(LogLevel lvl)
  {
    switch(lvl)
    {
      case eLogDebug:
        return "DBG";
      case eLogInfo:
        return "NFO";
      case eLogWarn:
        return "WRN";
      case eLogError:
        return "ERR";
      default:
        return "???";
    }
  }
}  // namespace llarp
