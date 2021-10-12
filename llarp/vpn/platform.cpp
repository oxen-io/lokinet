
#include <llarp/ev/vpn.hpp>

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

#include <exception>
#include <memory>

namespace llarp::vpn
{
  std::shared_ptr<Platform>
  MakeNativePlatform(llarp::Context* ctx)
  {
    (void)ctx;
    std::shared_ptr<Platform> plat;
#ifdef _WIN32
    plat = std::make_shared<Win32Platform>();
#endif
#ifdef __linux__
#ifdef ANDROID
    plat = std::make_shared<AndroidPlatform>(ctx);
#else
    plat = std::make_shared<LinuxPlatform>();
#endif
#endif
    if (plat == nullptr)
      throw std::runtime_error{"not supported"};
    return plat;
  }

  void
  CleanUpPlatform()
  {
#ifdef _WIN32
    LogInfo("cleaning up all our wintun jizz...");
    try
    {
      wintun::API{};
    }
    catch (...)
    {
      LogError("failed to clean up our wintun jizz?");
    }
#endif
  }
}  // namespace llarp::vpn
