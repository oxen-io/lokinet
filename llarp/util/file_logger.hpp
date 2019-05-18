#ifndef LLARP_UTIL_FILE_LOGGER_HPP
#define LLARP_UTIL_FILE_LOGGER_HPP

#include <util/logstream.hpp>
#include <util/thread_pool.hpp>
#include <util/time.hpp>

#include <deque>

namespace llarp
{
  /// flushable file based log stream
  struct FileLogStream : public ILogStream
  {
    FileLogStream(thread::ThreadPool* disk, FILE* f,
                  llarp_time_t flushInterval);

    ~FileLogStream();

    void
    PreLog(std::stringstream& out, LogLevel lvl, const char* fname,
           int lineno) const override;

    void
    Print(LogLevel, const char*, const std::string& msg) override;

    void
    Tick(llarp_time_t now) override;

    void
    PostLog(std::stringstream&) const override
    {
    }

   private:
    bool
    ShouldFlush(llarp_time_t now) const;

    void
    FlushLinesToDisk(llarp_time_t now);

    thread::ThreadPool* m_Disk;
    FILE* m_File;
    const llarp_time_t m_FlushInterval;
    llarp_time_t m_LastFlush = 0;
    std::deque< std::string > m_Lines;
  };
}  // namespace llarp

#endif
