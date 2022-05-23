#include "resolver.hpp"
#include <llarp/util/logging.hpp>
#include <llarp/constants/platform.hpp>

namespace llarp::dns
{
  class Null_SystemSettings : public I_SystemSettings
  {
    void
    set_resolver(std::string, llarp::SockAddr, bool) override
    {
      LogDebug("lokinet is not built with systemd support, cannot set systemd resolved DNS");
    }
  };
}  // namespace llarp::dns

#ifdef WITH_SYSTEMD
extern "C"
{
#include <net/if.h>
}

#include <llarp/linux/dbus.hpp>

using namespace std::literals;

namespace llarp::dns
{
  class SD_SystemSettings : public I_SystemSettings
  {
   public:
    void
    set_resolver(std::string ifname, llarp::SockAddr dns, bool global) override
    {
      unsigned int if_ndx = if_nametoindex(ifname.c_str());
      if (if_ndx == 0)
      {
        throw std::runtime_error{"No such interface '" + ifname + "'"};
      }

      linux::DBUS _dbus{
          "org.freedesktop.resolve1",
          "/org/freedesktop/resolve1",
          "org.freedesktop.resolve1.Manager"};
      // This passing address by bytes and using two separate calls for ipv4/ipv6 is gross, but
      // the alternative is to build up a bunch of crap with va_args, which is slightly more
      // gross.
      const bool isStandardDNSPort = dns.getPort() == 53;
      if (dns.isIPv6())
      {
        auto ipv6 = dns.getIPv6();
        static_assert(sizeof(ipv6) == 16);
        auto* a = reinterpret_cast<const uint8_t*>(&ipv6);
        if (isStandardDNSPort)
        {
          _dbus(
              "SetLinkDNS",
              "ia(iay)",
              (int32_t)if_ndx,
              (int)1,             // number of "iayqs"s we are passing
              (int32_t)AF_INET6,  // network address type
              (int)16,            // network addr byte size
                                  // clang-format off
              a[0], a[1], a[2],  a[3],  a[4],  a[5],  a[6],  a[7],
              a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15] // yuck
                                  // clang-format on
          );
        }
        else
        {
          _dbus(
              "SetLinkDNSEx",
              "ia(iayqs)",
              (int32_t)if_ndx,
              (int)1,             // number of "iayqs"s we are passing
              (int32_t)AF_INET6,  // network address type
              (int)16,            // network addr byte size
              // clang-format off
              a[0], a[1], a[2],  a[3],  a[4],  a[5],  a[6],  a[7],
              a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15], // yuck
              // clang-format on
              (uint16_t)dns.getPort(),
              nullptr  // dns server name (for TLS SNI which we don't care about)
          );
        }
      }
      else
      {
        auto ipv4 = dns.getIPv4();
        static_assert(sizeof(ipv4) == 4);
        auto* a = reinterpret_cast<const uint8_t*>(&ipv4);
        if (isStandardDNSPort)
        {
          _dbus(
              "SetLinkDNS",
              "ia(iay)",
              (int32_t)if_ndx,
              (int)1,            // number of "iayqs"s we are passing
              (int32_t)AF_INET,  // network address type
              (int)4,            // network addr byte size
                                 // clang-format off
              a[0], a[1], a[2], a[3] // yuck
                                 // clang-format on
          );
        }
        else
        {
          _dbus(
              "SetLinkDNSEx",
              "ia(iayqs)",
              (int32_t)if_ndx,
              (int)1,            // number of "iayqs"s we are passing
              (int32_t)AF_INET,  // network address type
              (int)4,            // network addr byte size
              // clang-format off
              a[0], a[1], a[2], a[3], // yuck
              // clang-format on
              (uint16_t)dns.getPort(),
              nullptr  // dns server name (for TLS SNI which we don't care about)
          );
        }
      }

      if (global)
        // Setting "." as a routing domain gives this DNS server higher priority in resolution
        // compared to dns servers that are set without a domain (e.g. the default for a
        // DHCP-configured DNS server)
        _dbus(
            "SetLinkDomains",
            "ia(sb)",
            (int32_t)if_ndx,
            (int)1,  // array size
            "."      // global DNS root
        );
      else
        // Only resolve .loki and .snode through lokinet (so you keep using your local DNS
        // server for everything else, which is nicer than forcing everything though lokinet's
        // upstream DNS).
        _dbus(
            "SetLinkDomains",
            "ia(sb)",
            (int32_t)if_ndx,
            (int)2,   // array size
            "loki",   // domain
            (int)1,   // routing domain = true
            "snode",  // domain
            (int)1    // routing domain = true
        );
    }
  };

  /// network manager dns setter
  class NM_SystemSettings : public I_SystemSettings
  {
   public:
    void
    set_resolver(std::string ifname, llarp::SockAddr dns, bool global) override
    {
      unsigned int if_ndx = if_nametoindex(ifname.c_str());
      if (if_ndx == 0)
      {
        throw std::runtime_error{"No such interface '" + ifname + "'"};
      }
      (void)dns;
      (void)global;
      // TODO: implement network manager shit
    }
  };

}  // namespace llarp::dns

#endif  // WITH_SYSTEMD

namespace llarp::dns
{
  class MultiSettings : public I_SystemSettings
  {
    std::vector<std::unique_ptr<I_SystemSettings>> m_Impls;

   public:
    void
    add_impl(std::unique_ptr<I_SystemSettings> impl)
    {
      m_Impls.emplace_back(std::move(impl));
    }

    void
    set_resolver(std::string ifname, llarp::SockAddr dns, bool global) override
    {
      size_t fails{0};
      for (const auto& ptr : m_Impls)
      {
        try
        {
          ptr->set_resolver(ifname, dns, global);
        }
        catch (std::exception& ex)
        {
          LogWarn(ex.what());
          fails++;
        }
      }
      if (fails == m_Impls.size())
        throw std::runtime_error{"tried all ways to set resolver and failed"};
    }
  };

  std::shared_ptr<I_SystemSettings>
  MakeSystemSettings()
  {
    auto settings = std::make_shared<MultiSettings>();
    settings->add_impl(std::make_unique<Null_SystemSettings>());
    if constexpr (llarp::platform::has_systemd)
    {
      settings->add_impl(std::make_unique<SD_SystemSettings>());
      settings->add_impl(std::make_unique<NM_SystemSettings>());
    }
    return settings;
  }
}  // namespace llarp::dns
