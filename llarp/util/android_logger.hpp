#ifndef LLARP_UTIL_OSTREAM_LOGGER_HPP
#define LLARP_UTIL_OSTREAM_LOGGER_HPP

#include <util/logstream.hpp>
#include <iostream>

namespace llarp
{
  struct AndroidLogStream : public ILogStream
  {
    void
    PreLog(std::stringstream& s, LogLevel lvl, const char* fname,
           int lineno) const override;

    void
    Log(LogLevel lvl, const std::string& msg) const override;
  };
}  // namespace llarp

#endif
