#ifndef LLARP_UTIL_LOG_LEVEL_HPP
#define LLARP_UTIL_LOG_LEVEL_HPP
#include <string>
#include <optional>

namespace llarp
{
  // probably will need to move out of llarp namespace for c api
  enum LogLevel
  {
    eLogTrace,
    eLogDebug,
    eLogInfo,
    eLogWarn,
    eLogError,
    eLogNone
  };

  std::string
  LogLevelToString(LogLevel level);

  std::string
  LogLevelToName(LogLevel lvl);

  std::optional<LogLevel>
  LogLevelFromString(std::string level);

}  // namespace llarp

#endif
