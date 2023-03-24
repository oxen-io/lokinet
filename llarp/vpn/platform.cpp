
#include "platform.hpp"
#include "llarp/config/config.hpp"
#include "llarp/net/net.hpp"

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

namespace llarp::vpn
{
  const llarp::net::Platform*
  IRouteManager::Net_ptr() const
  {
    return llarp::net::Platform::Default_ptr();
  }

  InterfaceInfo::InterfaceInfo(const NetworkConfig& net_conf, const net::Platform& net_plat)
      : ifname{net_conf.ifname(net_plat)}, index{}, dnsaddr{}, addrs{}
  {
    addrs.emplace_back(net_conf.ifaddr(net_plat));
  }

  std::shared_ptr<Platform>
  MakeNativePlatform(llarp::Context* ctx)
  {
    (void)ctx;
    std::shared_ptr<Platform> plat;
#ifdef _WIN32
    plat = std::make_shared<llarp::win32::VPNPlatform>(ctx);
#endif
#ifdef __linux__
#ifdef ANDROID
    plat = std::make_shared<vpn::AndroidPlatform>(ctx);
#else
    plat = std::make_shared<vpn::LinuxPlatform>();
#endif
#endif
#ifdef __APPLE__
    throw std::runtime_error{"not supported"};
#endif
    return plat;
  }

}  // namespace llarp::vpn
