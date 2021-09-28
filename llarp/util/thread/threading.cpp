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
      int rc{};
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
      /* on bsd this function has void return type */
      pthread_set_name_np(pthread_self(), name.c_str());
#elif defined(__MACH__)
      rc = pthread_setname_np(name.c_str());
#elif defined(__linux__) || defined(__sun)
      rc = pthread_setname_np(pthread_self(), name.c_str());
#elif defined(_WIN32)
      rc = pthread_setname_np(pthread_self(), name.c_str());
#else
#error "unspported platform"
#endif
      if (rc)
      {
        LogError(
            "Failed to set thread name to ", name, " errno = ", rc, " errstr = ", ::strerror(rc));
      }
    }
  }  // namespace util
}  // namespace llarp
