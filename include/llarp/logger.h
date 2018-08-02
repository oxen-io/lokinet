#ifndef LLARP_LOGGER_H
#define LLARP_LOGGER_H

extern "C"
{
  enum LogLevel
  {
    eLogDebug,
    eLogInfo,
    eLogWarn,
    eLogError
  };

  void
  cSetLogLevel(enum LogLevel lvl);

  void
  cSetLogNodeName(const char* name);
}

#endif
