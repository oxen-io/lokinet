
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

#ifdef __FreeBSD__
#include "freebsd.hpp"
#endif

#include <exception>
#include <memory>

// undef'd when we have a vpn platform
// if set we short circuit compilation with an error
#define NO_VPN_PLAT

namespace llarp::vpn
{
  std::shared_ptr<Platform>
  MakeNativePlatform(llarp::Context* ctx)
  {
#ifdef _WIN32
#undef NO_VPN_PLAT
    using namespace vpn::win32;
#endif
#ifdef __linux__
#ifdef ANDROID
#undef NO_VPN_PLAT
    using namespace vpn::android;
#else

#undef NO_VPN_PLAT
    using namespace vpn::linux;
#endif
#endif
#ifdef __APPLE__

#undef NO_VPN_PLAT
    struct VPNPlatform
    {};
    throw std::runtime_error{"not supported"};
#endif
#ifdef __FreeBSD__
#undef NO_VPN_PLAT
    using namespace vpn::freebsd;
#endif

#ifdef NO_VPN_PLAT
#error "no vpn platform implemented for this target"
#else
    return std::make_shared<VPNPlatform>(ctx);
#endif
  }

  void
  CleanUpPlatform()
  {
#ifdef _WIN32
    try
    {
      wintun::API api{};
      api.CleanUpForUninstall();
    }
    catch (std::exception& ex)
    {
      LogError("failed to clean up our wintun jizz? ", ex.what());
    }
#endif
  }
}  // namespace llarp::vpn
