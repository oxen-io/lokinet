#include "threading.hpp"

#include <llarp/util/logging/logger.hpp>
#include <cstring>

#include <pthread.h>
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <pthread_np.h>
#endif

namespace llarp::util
{
  void
  SetThreadName(const std::string& name)
  {
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    /* on bsd this function has void return type */
    auto set_name = [](auto name) {
      pthread_set_name_np(pthread_self(), name.c_str());
      return 0;
    };
#endif
#if defined(__APPLE__)
    auto set_name = [](auto name) { return pthread_setname_np(name.c_str()); };
#endif
#if defined(__linux__) || defined(__sun) || defined(__MINGW32__)
    auto set_name = [](auto name) { return pthread_setname_np(pthread_self(), name.c_str()); };
#endif
    if (auto rc = set_name(name))
      LogError("Failed to set thread name to '", name, "' :", ::strerror(rc));
  }
}  // namespace llarp::util
