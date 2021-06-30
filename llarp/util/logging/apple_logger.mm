#ifdef __APPLE__
#include "apple_logger.hpp"
#include "logger_internal.hpp"

#include <Foundation/Foundation.h>

namespace llarp
{
  void
  NSLogStream::PreLog(
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
  NSLogStream::Print(LogLevel, const char*, const std::string& msg)
  {
    const char* msg_ptr = msg.c_str();
    const char* msg_fmt = "%s";
    NSString* fmt = [[NSString alloc] initWithUTF8String:msg_ptr];
    NSString* str = [[NSString alloc] initWithUTF8String:msg_fmt];
    NSLog(fmt, str);
  }

  void
  NSLogStream::PostLog(std::stringstream&) const
  {}

}  // namespace llarp
#endif
