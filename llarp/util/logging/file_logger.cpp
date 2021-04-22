#include <chrono>
#include "file_logger.hpp"
#include "logger_internal.hpp"
#include <llarp/util/time.hpp>

#include <utility>

namespace llarp
{
  void
  FileLogStream::Flush(Lines_t* lines, FILE* const f)
  {
    bool wrote_stuff = false;
    do
    {
      auto maybe_line = lines->tryPopFront();
      if (not maybe_line)
        break;
      if (fprintf(f, "%s\n", maybe_line->c_str()) >= 0)
        wrote_stuff = true;
    } while (true);

    if (wrote_stuff)
      fflush(f);
  }

  // namespace
  FileLogStream::FileLogStream(
      std::function<void(Work_t)> disk, FILE* f, llarp_time_t flushInterval, bool closeFile)
      : m_Lines(1024 * 8)
      , m_Disk(std::move(disk))
      , m_File(f)
      , m_FlushInterval(flushInterval)
      , m_Close(closeFile)
  {
    m_Lines.enable();
  }

  FileLogStream::~FileLogStream()
  {
    m_Lines.disable();
    do
    {
      auto line = m_Lines.tryPopFront();
      if (not line)
        break;
    } while (true);
    fflush(m_File);
    if (m_Close)
      fclose(m_File);
  }

  bool
  FileLogStream::ShouldFlush(llarp_time_t now) const
  {
    if (m_Lines.full())
      return true;
    if (m_LastFlush >= now)
      return false;
    const auto dlt = now - m_LastFlush;
    return dlt >= m_FlushInterval;
  }

  void
  FileLogStream::PreLog(
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
  FileLogStream::Print(LogLevel, const char*, const std::string& msg)
  {
    m_Lines.pushBack(msg);
  }

  void
  FileLogStream::AppendLog(
      LogLevel lvl,
      const char* fname,
      int lineno,
      const std::string& nodename,
      const std::string msg)
  {
    ILogStream::AppendLog(lvl, fname, lineno, nodename, msg);
    Tick(llarp::time_now_ms());
  }

  void
  FileLogStream::ImmediateFlush()
  {
    Flush(&m_Lines, m_File);
    m_LastFlush = time_now_ms();
  }

  void
  FileLogStream::Tick(llarp_time_t now)
  {
    if (ShouldFlush(now))
      FlushLinesToDisk(now);
  }

  void
  FileLogStream::FlushLinesToDisk(llarp_time_t now)
  {
    FILE* const f = m_File;
    auto lines = &m_Lines;
    m_Disk([f, lines]() { Flush(lines, f); });
    m_LastFlush = now;
  }
}  // namespace llarp
