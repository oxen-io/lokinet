
#include <llarp/vpn/null.hpp>
#ifdef _WIN32
#include "win32.hpp"
#endif
#ifdef __linux__
#ifdef ANDROID
#include "android.hpp"
#else
#include "linux.hpp"
#endif
#endif

namespace llarp::vpn
{
  std::shared_ptr<Platform>
  MakeNativePlatform(llarp::Context* ctx)
  {
    (void)ctx;
    auto nullplat = std::make_shared<NullPlatform>();
    std::shared_ptr<Platform> plat = nullplat;
#if defined(_WIN32)
    plat = std::make_shared<Win32Platform>();
#elif defined(ANDROID)
    plat = std::make_shared<AndroidPlatform>(ctx);
#elif defined(__linux__)
    plat = std::make_shared<LinuxPlatform>();
#endif
    return plat;
  }

}  // namespace llarp::vpn
