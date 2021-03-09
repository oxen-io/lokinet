#ifndef _WIN32
#include "logger_syslog.hpp"

#include "logger_internal.hpp"

#include <syslog.h>
namespace llarp
{
  void
  SysLogStream::PreLog(
      std::stringstream& ss,
      LogLevel lvl,
      const char* fname,
      int lineno,
      const std::string& nodename) const
  {
    ss << "[" << LogLevelToString(lvl) << "] ";
    ss << "[" << nodename << "]"
       << "(" << thread_id_string() << ") " << log_timestamp() << " " << fname << ":" << lineno
       << "\t";
  }

  void
  SysLogStream::Print(LogLevel lvl, const char*, const std::string& msg)
  {
    switch (lvl)
    {
      case eLogNone:
        return;
      case eLogTrace:
      case eLogDebug:
        ::syslog(LOG_DEBUG, "%s", msg.c_str());
        return;
      case eLogInfo:
        ::syslog(LOG_INFO, "%s", msg.c_str());
        return;
      case eLogWarn:
        ::syslog(LOG_WARNING, "%s", msg.c_str());
        return;
      case eLogError:
        ::syslog(LOG_ERR, "%s", msg.c_str());
        return;
    }
  }

  void
  SysLogStream::PostLog(std::stringstream&) const
  {}

}  // namespace llarp
#endif
