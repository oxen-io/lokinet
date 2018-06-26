#ifndef LLARP_LOGGER_H
#define LLARP_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

enum LogLevel
{
  eLogDebug,
  eLogInfo,
  eLogWarn,
  eLogError
};

void
cSetLogLevel(enum LogLevel lvl);

#ifdef __cplusplus
}
#endif

#endif
