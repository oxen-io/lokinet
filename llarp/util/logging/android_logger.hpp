#pragma once

#include "logstream.hpp"

#include <iostream>

namespace llarp
{
  struct AndroidLogStream : public ILogStream
  {
    void
    PreLog(
        std::stringstream& s,
        LogLevel lvl,
        const char* fname,
        int lineno,
        const std::string& nodename) const override;

    void
    Print(LogLevel lvl, const char* filename, const std::string& msg) override;

    void
    PostLog(std::stringstream&) const override;

    void Tick(llarp_time_t) override;

    void
    ImmediateFlush() override{};
  };
}  // namespace llarp
