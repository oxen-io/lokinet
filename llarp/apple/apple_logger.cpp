#include "apple_logger.hpp"

namespace llarp::apple
{
  void
  NSLogStream::PreLog(
      std::stringstream& ss,
      LogLevel lvl,
      std::string_view fname,
      int lineno,
      const std::string& nodename) const
  {
    ss << "[" << LogLevelToString(lvl) << "] ";
    ss << "[" << nodename << "]"
       << "(" << thread_id_string() << ") " << log_timestamp() << " " << fname << ":" << lineno
       << "\t";
  }

  void
  NSLogStream::Print(LogLevel, std::string_view, const std::string& msg)
  {
    ns_logger(msg.c_str());
  }

}  // namespace llarp::apple
