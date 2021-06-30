#pragma once
#ifdef __APPLE__
#include "logstream.hpp"

namespace llarp
{
  struct NSLogStream : public ILogStream
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

    virtual void
    ImmediateFlush() override
    {}

    void Tick(llarp_time_t) override
    {}
  };
}  // namespace llarp

#endif
