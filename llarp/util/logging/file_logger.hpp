#ifndef LLARP_UTIL_FILE_LOGGER_HPP
#define LLARP_UTIL_FILE_LOGGER_HPP

#include <util/logging/logstream.hpp>

#include <util/thread/thread_pool.hpp>
#include <util/thread/queue.hpp>
#include <util/time.hpp>

#include <deque>

namespace llarp
{
  /// flushable file based log stream
  struct FileLogStream : public ILogStream
  {
    FileLogStream(
        std::shared_ptr<thread::ThreadPool> disk,
        FILE* f,
        llarp_time_t flushInterval,
        bool closefile = true);

    ~FileLogStream() override;

    void
    PreLog(
        std::stringstream& out,
        LogLevel lvl,
        const char* fname,
        int lineno,
        const std::string& nodename) const override;

    void
    Print(LogLevel, const char*, const std::string& msg) override;

    void
    Tick(llarp_time_t now) override;

    void
    PostLog(std::stringstream&) const override{};

    void
    AppendLog(
        LogLevel lvl,
        const char* fname,
        int lineno,
        const std::string& nodename,
        const std::string msg) override;

    virtual void
    ImmediateFlush() override;

    using Lines_t = thread::Queue< std::string >;

   protected:
    Lines_t m_Lines;

   private:
    static void
    Flush(Lines_t* const, FILE* const);

    bool
    ShouldFlush(llarp_time_t now) const;

    void
    FlushLinesToDisk(llarp_time_t now);

    std::shared_ptr<thread::ThreadPool> m_Disk;
    FILE* const m_File;
    const llarp_time_t m_FlushInterval;
    llarp_time_t m_LastFlush = 0s;
    const bool m_Close;
  };
}  // namespace llarp

#endif
