#include "apple_logger.hpp"

namespace llarp::apple
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
    ns_logger(msg.c_str());
  }

}  // namespace llarp::apple
