#ifndef LLARP_UTIL_LOGGER_SYSLOG_HPP
#define LLARP_UTIL_LOGGER_SYSLOG_HPP

#include <util/logging/logstream.hpp>
#include <iostream>

namespace llarp
{
  struct SysLogStream : public ILogStream
  {
    void
    PreLog(
        std::stringstream& s,
        LogLevel lvl,
        const char* fname,
        int lineno,
        const std::string& nodename) const override;

    void
    Print(LogLevel lvl, const char* tag, const std::string& msg) override;

    void
    PostLog(std::stringstream& ss) const override;

    void Tick(llarp_time_t) override
    {
    }
  };
}  // namespace llarp
#endif
