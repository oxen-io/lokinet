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
    FileLogStream(thread::ThreadPool* disk, FILE* f, llarp_time_t flushInterval,
                  bool closefile = true);

    ~FileLogStream() override;

    void
    PreLog(std::stringstream& ss, LogLevel lvl, const char* fname, int lineno,
           const std::string& nodename) const override;

    void
    Print(LogLevel /*lvl*/, const char* /*filename*/, const std::string& msg) override;

    void
    Tick(llarp_time_t now) override;

    void
    PostLog(std::stringstream& /*out*/) const override
    {
    }

   protected:
    std::deque< std::string > m_Lines;

   private:
    bool
    ShouldFlush(llarp_time_t now) const;

    void
    FlushLinesToDisk(llarp_time_t now);

    thread::ThreadPool* m_Disk;
    FILE* const m_File;
    const llarp_time_t m_FlushInterval;
    llarp_time_t m_LastFlush = 0;
    const bool m_Close;
  };
}  // namespace llarp

#endif
