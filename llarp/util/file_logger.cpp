#include <util/file_logger.hpp>
#include <util/logger_internal.hpp>

namespace llarp
{
  namespace
  {
    static void
    Flush(const std::deque< std::string > &lines, FILE *const f)
    {
      for(const auto &line : lines)
        fprintf(f, "%s\n", line.c_str());
      fflush(f);
    }
  }  // namespace
  FileLogStream::FileLogStream(thread::ThreadPool *disk, FILE *f,
                               llarp_time_t flushInterval)
      : m_Disk(disk), m_File(f), m_FlushInterval(flushInterval)
  {
  }

  FileLogStream::~FileLogStream()
  {
    fflush(m_File);
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
    m_Disk->addJob(std::bind(&Flush, std::move(m_Lines), m_File));
    m_LastFlush = now;
  }
}  // namespace llarp
