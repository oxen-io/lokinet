#include <util/logging/loglevel.hpp>
#include <unordered_map>

namespace llarp
{
  std::string
  LogLevelToString(LogLevel lvl)
  {
    switch(lvl)
    {
      case eLogTrace:
        return "TRC";
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

  std::string
  LogLevelToName(LogLevel lvl)
  {
    switch(lvl)
    {
      case eLogTrace:
        return "Trace";
      case eLogDebug:
        return "Debug";
      case eLogInfo:
        return "Info";
      case eLogWarn:
        return "Warn";
      case eLogError:
        return "Error";
      case eLogNone:
        return "Off";
      default:
        return "???";
    }
  }

  absl::optional< LogLevel >
  LogLevelFromString(std::string level)
  {
    std::transform(level.begin(), level.end(), level.begin(),
                   [](const char ch) -> char { return std::tolower(ch); });
    static const std::unordered_map< std::string, LogLevel > levels = {
        {"trace", eLogTrace}, {"debug", eLogDebug}, {"info", eLogInfo},
        {"warn", eLogWarn},   {"error", eLogError}, {"none", eLogNone}};

    const auto itr = levels.find(level);
    if(itr == levels.end())
      return {};
    return itr->second;
  }
}  // namespace llarp
