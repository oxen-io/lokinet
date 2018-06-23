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
}

extern "C" {
void
cSetLogLevel(LogLevel lvl)
{
  llarp::SetLogLevel((llarp::LogLevel)lvl);
}

}
