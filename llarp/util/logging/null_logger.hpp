
#ifndef LLARP_UTIL_LOGGING_NULL_LOGGER_HPP
#define LLARP_UTIL_LOGGING_NULL_LOGGER_HPP
#include <util/logging/logstream.hpp>
#include <iostream>

namespace llarp
{
  /// This log stream does nothing
  struct NullLogStream : public ILogStream
  {
    void
    PreLog(std::stringstream &, LogLevel, const char *, int,
           const std::string &) const override{};
    void
    Print(LogLevel, const char *, const std::string &) override{};

    void
    PostLog(std::stringstream &) const override{};

    void Tick(llarp_time_t) override{};
  };
}  // namespace llarp

#endif
