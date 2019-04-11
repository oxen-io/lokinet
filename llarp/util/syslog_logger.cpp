#ifndef _WIN32
#include <util/logger_syslog.hpp>
#include <util/logger_internal.hpp>
#include <syslog.h>
namespace llarp
{
  void
  SysLogStream::PreLog(std::stringstream& ss, LogLevel lvl, const char* fname,
                       int lineno) const
  {
    switch(lvl)
    {
      case eLogNone:
        break;
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

    ss << "(" << thread_id_string() << ") " << log_timestamp() << " " << fname
       << ":" << lineno << "\t";
  }

  void
  SysLogStream::Print(LogLevel lvl, const char*, const std::string& msg) const
  {
    switch(lvl)
    {
      case eLogNone:
        return;
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
  {
  }
}  // namespace llarp
#endif
