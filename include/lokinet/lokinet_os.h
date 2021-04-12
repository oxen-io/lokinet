#pragma once

/// OS specific types

#ifdef _WIN32

#else
#include <sys/uio.h>
#include <poll.h>

#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef _WIN32
  typedef HANDLE OS_FD_t;
#else
typedef int OS_FD_t;
#endif

#ifdef __cplusplus
}
#endif
