#include "ostream_logger.hpp"
#include "logger_internal.hpp"

namespace llarp
{
  OStreamLogStream::OStreamLogStream(bool withColours, std::ostream& out)
      : m_withColours(withColours), m_Out(out)
  {}

  void
  OStreamLogStream::PreLog(
      std::stringstream& ss,
      LogLevel lvl,
      std::string_view filename,
      int lineno,
      const std::string& nodename) const
  {
    if (m_withColours)
    {
      switch (lvl)
      {
        case eLogNone:
          return;
        case eLogTrace:
        case eLogDebug:
          ss << (char)27 << "[0m";
          break;
        case eLogInfo:
          ss << (char)27 << "[1m";
          break;
        case eLogWarn:
          ss << (char)27 << "[1;33m";
          break;
        case eLogError:
          ss << (char)27 << "[1;31m";
          break;
      }
    }
    ss << "[" << LogLevelToString(lvl) << "] ";
    ss << "[" << nodename << "]"
       << "(" << thread_id_string() << ") " << log_timestamp() << " " << filename << ":" << lineno
       << "\t";
  }

  void
  OStreamLogStream::PostLog(std::stringstream& ss) const
  {
    if (m_withColours)
      ss << (char)27 << "[0;0m";
    ss << std::endl;
  }

  void
  OStreamLogStream::Print(LogLevel, std::string_view, const std::string& msg)
  {
    m_Out << msg << std::flush;
  }

  void
  OStreamLogStream::ImmediateFlush()
  {
    m_Out << std::flush;
  }

}  // namespace llarp
