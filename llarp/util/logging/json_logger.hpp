#ifndef LLARP_UTIL_JSON_LOGGER
#define LLARP_UTIL_JSON_LOGGER

#include "file_logger.hpp"

namespace llarp
{
  struct JSONLogStream : public FileLogStream
  {
    JSONLogStream(
        std::function<void(FileLogStream::Work_t)> disk,
        FILE* f,
        llarp_time_t flushInterval,
        bool closeFile)
        : FileLogStream(std::move(disk), f, flushInterval, closeFile)
    {}

    void
    AppendLog(
        LogLevel lvl,
        const char* fname,
        int lineno,
        const std::string& nodename,
        const std::string msg) override;
  };
}  // namespace llarp

#endif
