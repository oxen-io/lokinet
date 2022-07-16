#include "systemd_resolved.hpp"
#include <llarp/util/logging.hpp>

#ifndef WITH_SYSTEMD

namespace llarp
{
  bool
  systemd_resolved_set_dns(std::string, llarp::SockAddr, bool)
  {
    LogDebug("lokinet is not built with systemd support, cannot set systemd resolved DNS");
    return false;
  }
}  // namespace llarp

#else

#include <stdexcept>

extern "C"
{
#include <systemd/sd-bus.h>
#include <net/if.h>
}

using namespace std::literals;

namespace llarp
{
  namespace
  {
    template <typename... T>
    void
    resolved_call(sd_bus* bus, const char* method, const char* arg_format, T... args)
    {
      sd_bus_error error = SD_BUS_ERROR_NULL;
      sd_bus_message* msg = nullptr;
      int r = sd_bus_call_method(
          bus,
          "org.freedesktop.resolve1",
          "/org/freedesktop/resolve1",
          "org.freedesktop.resolve1.Manager",
          method,
          &error,
          &msg,
          arg_format,
          args...);

      if (r < 0)
        throw std::runtime_error{"sdbus resolved "s + method + " failed: " + strerror(-r)};

      sd_bus_message_unref(msg);
      sd_bus_error_free(&error);
    }

    struct sd_bus_deleter
    {
      void
      operator()(sd_bus* ptr) const
      {
        sd_bus_unref(ptr);
      }
    };
  }  // namespace

  bool
  systemd_resolved_set_dns(std::string ifname, llarp::SockAddr dns, bool global)
  {
    unsigned int if_ndx = if_nametoindex(ifname.c_str());
    if (if_ndx == 0)
    {
      LogWarn("No such interface '", ifname, "'");
      return false;
    }

    // Connect to the system bus
    sd_bus* bus = nullptr;
    int r = sd_bus_open_system(&bus);
    if (r < 0)
    {
      LogWarn("Failed to connect to system bus to set DNS: ", strerror(-r));
      return false;
    }
    std::unique_ptr<sd_bus, sd_bus_deleter> bus_ptr{bus};

    try
    {
      // This passing address by bytes and using two separate calls for ipv4/ipv6 is gross, but the
      // alternative is to build up a bunch of crap with va_args, which is slightly more gross.
      const bool isStandardDNSPort = dns.getPort() == 53;
      if (dns.isIPv6())
      {
        auto ipv6 = dns.getIPv6();
        static_assert(sizeof(ipv6) == 16);
        auto* a = reinterpret_cast<const uint8_t*>(&ipv6);
        if (isStandardDNSPort)
        {
          resolved_call(
              bus,
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
          resolved_call(
              bus,
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
          resolved_call(
              bus,
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
          resolved_call(
              bus,
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
        resolved_call(
            bus,
            "SetLinkDomains",
            "ia(sb)",
            (int32_t)if_ndx,
            (int)1,  // array size
            "."      // global DNS root
        );
      else
        // Only resolve .loki and .snode through lokinet (so you keep using your local DNS server
        // for everything else, which is nicer than forcing everything though lokinet's upstream
        // DNS).
        resolved_call(
            bus,
            "SetLinkDomains",
            "ia(sb)",
            (int32_t)if_ndx,
            (int)2,   // array size
            "loki",   // domain
            (int)1,   // routing domain = true
            "snode",  // domain
            (int)1    // routing domain = true
        );

      return true;
    }
    catch (const std::exception& e)
    {
      LogWarn("Failed to set DNS via systemd-resolved: ", e.what());
    }
    return false;
  }

}  // namespace llarp

#endif  // WITH_SYSTEMD
