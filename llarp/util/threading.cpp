#include <util/threading.hpp>

#include <util/logger.hpp>

#ifdef POSIX
#include <pthread.h>
#ifdef __FreeBSD__
#include <pthread_np.h>
#endif
#endif

namespace llarp
{
  namespace util
  {
    void
    SetThreadName(const std::string& name)
    {
#ifdef POSIX
#ifdef __MACH__
      const int rc = pthread_setname_np(name.c_str());
#elif defined(__FreeBSD__)
      const int rc = pthread_setname_np(pthread_self(), name.c_str(), nullptr);
#else
      const int rc = pthread_setname_np(pthread_self(), name.c_str());
#endif
      if(rc)
      {
        LogError("Failed to set thread name to ", name, " errno = ", rc,
                 " errstr = ", strerror(rc));
      }
#else
      LogInfo("Thread name setting not supported on this platform");
      (void)name;
#endif
    }
  }  // namespace util
}  // namespace llarp
