#ifdef _WIN32
#include <vpn/win32.hpp>
#endif
#ifdef __linux__
#ifdef ANDROID
#include <vpn/android.hpp>
#else
#include <vpn/linux.hpp>
#endif
#endif
#ifdef __APPLE__
#include <vpn/apple.hpp>
#endif

namespace llarp::vpn
{
  std::unique_ptr<Platform>
  MakePlatform(llarp::Context* ctx)
  {
    (void)ctx;
    std::unique_ptr<Platform> plat;
#ifdef _WIN32
    plat = std::make_unique<vpn::Win32Platform>();
#endif
#ifdef __linux__
#ifdef ANDROID
    plat = std::make_unique<vpn::AndroidPlatform>();
#else
    plat = std::make_unique<vpn::LinuxPlatform>();
#endif
#endif
#ifdef __APPLE__
    plat = std::make_unique<vpn::ApplePlatform>();
#endif
    return plat;
  }
}  // namespace llarp::vpn
