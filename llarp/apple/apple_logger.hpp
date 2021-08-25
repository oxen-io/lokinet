#pragma once

#include <llarp/util/logging/logger.hpp>
#include <llarp/util/logging/logstream.hpp>

namespace llarp::apple
{
  struct NSLogStream : public ILogStream
  {
    using ns_logger_callback = void (*)(const char* log_this);

    NSLogStream(ns_logger_callback logger) : ns_logger{logger}
    {}

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
    PostLog(std::stringstream& ss) const override
    {}

    void
    ImmediateFlush() override
    {}

    void Tick(llarp_time_t) override
    {}

   private:
    ns_logger_callback ns_logger;
  };
}  // namespace llarp::apple
