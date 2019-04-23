#ifndef LLARP_UTIL_FILE_LOGGER_HPP
#define LLARP_UTIL_FILE_LOGGER_HPP

#include <util/logstream.hpp>
#include <util/threadpool.h>
#include <util/time.hpp>

namespace llarp
{
  /// fluhsable file based log stream
  struct FileLogStream : public ILogStream
  {
    FileLogStream(llarp_threadpool* disk, FILE* f, llarp_time_t flushInterval);

    ~FileLogStream();

    void
    PreLog(std::stringstream& out, LogLevel lvl, const char* fname,
           int lineno) const override;

    void
    Print(LogLevel, const char*, const std::string& msg) override;

    void
    Tick(llarp_time_t now) override;

    void
    PostLog(std::stringstream&) const override{};

   private:
    struct FlushEvent
    {
      FlushEvent(std::deque< std::string > l, FILE* file)
          : lines(std::move(l)), f(file)
      {
      }

      const std::deque< std::string > lines;
      FILE* const f;

      void
      Flush();
      static void
      HandleFlush(void*);
    };

    bool
    ShouldFlush(llarp_time_t now) const;

    void
    FlushLinesToDisk(llarp_time_t now);

    llarp_threadpool* m_Disk;
    FILE* m_File;
    const llarp_time_t m_FlushInterval;
    llarp_time_t m_LastFlush = 0;
    std::deque< std::string > m_Lines;
  };
}  // namespace llarp

#endif