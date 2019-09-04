#include <util/logging/file_logger.hpp>
#include <util/logging/logger_internal.hpp>

#include <utility>

namespace llarp
{
  namespace
  {
    static void
    Flush(std::deque< std::string > lines, FILE *const f)
    {
      for(const auto &line : lines)
        fprintf(f, "%s\n", line.c_str());
      fflush(f);
    }
  }  // namespace
  FileLogStream::FileLogStream(std::shared_ptr< thread::ThreadPool > disk,
                               FILE *f, llarp_time_t flushInterval,
                               bool closeFile)
      : m_Disk(std::move(disk))
      , m_File(f)
      , m_FlushInterval(flushInterval)
      , m_Close(closeFile)
  {
  }

  FileLogStream::~FileLogStream()
  {
    fflush(m_File);
    if(m_Close)
      fclose(m_File);
  }

  bool
  FileLogStream::ShouldFlush(llarp_time_t now) const
  {
    if(m_LastFlush >= now)
      return false;
    const auto dlt = now - m_LastFlush;
    return dlt >= m_FlushInterval;
  }

  void
  FileLogStream::PreLog(std::stringstream &ss, LogLevel lvl, const char *fname,
                        int lineno, const std::string &nodename) const
  {
    ss << "[" << LogLevelToString(lvl) << "] ";
    ss << "[" << nodename << "]"
       << "(" << thread_id_string() << ") " << log_timestamp() << " " << fname
       << ":" << lineno << "\t";
  }

  void
  FileLogStream::Print(LogLevel, const char *, const std::string &msg)
  {
    m_Lines.emplace_back(msg);
  }

  void
  FileLogStream::Tick(llarp_time_t now)
  {
    if(ShouldFlush(now))
      FlushLinesToDisk(now);
  }

  void
  FileLogStream::FlushLinesToDisk(llarp_time_t now)
  {
    FILE *const f = m_File;
    std::deque< std::string > lines(m_Lines);
    m_Disk->addJob([=]() { Flush(lines, f); });
    m_Lines.clear();
    m_LastFlush = now;
  }
}  // namespace llarp
