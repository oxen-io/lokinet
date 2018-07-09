#ifndef LLARP_LOGGER_H
#define LLARP_LOGGER_H

enum LogLevel
{
  eLogDebug,
  eLogInfo,
  eLogWarn,
  eLogError
};

void
cSetLogLevel(enum LogLevel lvl);

#endif
