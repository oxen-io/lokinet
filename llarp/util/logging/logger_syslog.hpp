#pragma once

#include "logstream.hpp"
#include <iostream>

namespace llarp
{
  struct SysLogStream : public ILogStream
  {
    void
    PreLog(
        std::stringstream& s,
        LogLevel lvl,
        std::string_view filename,
        int lineno,
        const std::string& nodename) const override;

    void
    Print(LogLevel lvl, std::string_view tag, const std::string& msg) override;

    void
    PostLog(std::stringstream& ss) const override;

    virtual void
    ImmediateFlush() override
    {}

    void Tick(llarp_time_t) override
    {}
  };
}  // namespace llarp
