#include "threading.hpp"

#include <llarp/util/logging/logger.hpp>
#include <cstring>

#ifdef POSIX
#include <pthread.h>
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <pthread_np.h>
#endif
#endif

#ifdef _MSC_VER
#include <windows.h>
extern "C" void
SetThreadName(DWORD dwThreadID, LPCSTR szThreadName);
#endif

namespace llarp
{
  namespace util
  {
    void
    SetThreadName(const std::string& name)
    {
#ifdef POSIX
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
      /* on bsd this function has void return type */
      pthread_set_name_np(pthread_self(), name.c_str());
#else
#if defined(__MACH__)
      const int rc = pthread_setname_np(name.c_str());
// API present upstream since v2.11.3 and imported downstream
// in CR 8158 <https://www.illumos.org/issues/8158>
// We only use the native function on Microsoft C++ builds
#elif defined(__linux__) || defined(__sun)
      const int rc = pthread_setname_np(pthread_self(), name.c_str());
#else
#error "unsupported platform"
#endif
      if (rc)
      {
        LogError(
            "Failed to set thread name to ", name, " errno = ", rc, " errstr = ", ::strerror(rc));
      }
#endif
#elif _MSC_VER
      ::SetThreadName(::GetCurrentThreadId(), name.c_str());
#elif __MINGW32__
      const int rc = pthread_setname_np(pthread_self(), name.c_str());
#else
      LogInfo("Thread name setting not supported on this platform");
      (void)name;
#endif
    }
  }  // namespace util
}  // namespace llarp
