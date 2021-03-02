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
  std::shared_ptr<Platform>
  MakeNativePlatform(llarp::Context* ctx)
  {
    (void)ctx;
    std::shared_ptr<Platform> plat;
#ifdef _WIN32
    plat = std::make_shared<vpn::Win32Platform>();
#endif
#ifdef __linux__
#ifdef ANDROID
    plat = std::make_shared<vpn::AndroidPlatform>(ctx);
#else
    plat = std::make_shared<vpn::LinuxPlatform>();
#endif
#endif
#ifdef __APPLE__
    plat = std::make_shared<vpn::ApplePlatform>();
#endif
    return plat;
  }

}  // namespace llarp::vpn
