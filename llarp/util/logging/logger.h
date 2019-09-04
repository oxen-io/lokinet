#ifndef LLARP_LOGGER_H
#define LLARP_LOGGER_H

#ifdef __cplusplus
extern "C"
{
  enum LogLevel
  {
    eLogDebug,
    eLogInfo,
    eLogWarn,
    eLogError,
    eLogNone
  };

  void
  cSetLogLevel(enum LogLevel lvl);

  void
  cSetLogNodeName(const char* name);
}
#endif

#endif
