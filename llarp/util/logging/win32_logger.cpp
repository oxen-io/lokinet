#if defined(_WIN32)
#include "win32_logger.hpp"
#include "logger_internal.hpp"

static CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
static short old_attrs;

namespace llarp
{
  Win32LogStream::Win32LogStream(std::ostream& out) : OStreamLogStream(true, out), m_Out(out)
  {
    // Attempt to use ANSI escapes directly
    // if the modern console is active.
    DWORD mode_flags;

    GetConsoleMode(fd1, &mode_flags);
    // since release SDKs don't have ANSI escape support yet
    // we get all or nothing: if we can't get it, then we wouldn't
    // be able to get any of them individually
    mode_flags |= 0x0004 | 0x0008;
    BOOL t = SetConsoleMode(fd1, mode_flags);
    if (!t)
      this->isConsoleModern = false;  // fall back to setting colours manually
  }

  void
  Win32LogStream::PreLog(
      std::stringstream& ss,
      LogLevel lvl,
      const char* fname,
      int lineno,
      const std::string& nodename) const
  {
    if (!isConsoleModern)
    {
      switch (lvl)
      {
        case eLogNone:
          break;
        case eLogTrace:
          ss << "[TRC] ";
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
      ss << "[" << nodename << "]"
         << "(" << thread_id_string() << ") " << log_timestamp() << " " << fname << ":" << lineno
         << "\t";
    }
    else
      OStreamLogStream::PreLog(ss, lvl, fname, lineno, nodename);
  }

  void
  Win32LogStream::PostLog(std::stringstream& ss) const
  {
    if (!isConsoleModern)
      ss << std::endl;
    else
      OStreamLogStream::PostLog(ss);
  }

  void
  Win32LogStream::Print(LogLevel lvl, const char*, const std::string& msg)
  {
    if (!isConsoleModern)
    {
      GetConsoleScreenBufferInfo(fd1, &consoleInfo);
      old_attrs = consoleInfo.wAttributes;
      switch (lvl)
      {
        case eLogNone:
          break;
        case eLogTrace:
        case eLogDebug:
          SetConsoleTextAttribute(
              fd1,
              FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);  // low white on black
          break;
        case eLogInfo:
          SetConsoleTextAttribute(
              fd1,
              FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN
                  | FOREGROUND_BLUE);  // high white on black
          break;
        case eLogWarn:
          SetConsoleTextAttribute(
              fd1,
              FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);  // bright yellow
          break;
        case eLogError:
          SetConsoleTextAttribute(fd1, FOREGROUND_RED | FOREGROUND_INTENSITY);  // bright red
          break;
      }
    }

    m_Out << msg << std::flush;

    if (!isConsoleModern)
    {
      SetConsoleTextAttribute(fd1, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }
  }

}  // namespace llarp
#endif
