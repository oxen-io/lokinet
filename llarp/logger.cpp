#include "logger.hpp"
#include <llarp/logger.h>

namespace llarp
{
  Logger _glog;

  void
  SetLogLevel(LogLevel lvl)
  {
    _glog.minlevel = lvl;
  }
}  // namespace llarp

extern "C"
{
  void
  cSetLogLevel(LogLevel lvl)
  {
    llarp::SetLogLevel((llarp::LogLevel)lvl);
  }

  void
  cSetLogNodeName(const char* name)
  {
    llarp::_glog.nodeName = name;
  }
}