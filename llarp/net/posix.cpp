#include "net.hpp"
#include "net_if.hpp"
#include <stdexcept>
#include <llarp/constants/platform.hpp>

#include <arpa/inet.h>

#include "ip.hpp"
#include "ip_range.hpp"
#include <llarp/util/logging.hpp>
#include <llarp/util/str.hpp>

#ifdef ANDROID
#include <llarp/android/ifaddrs.h>
#else
#include <ifaddrs.h>
#endif

#include <cstdio>
#include <list>
#include <type_traits>

namespace llarp::net
{
#ifdef _WIN32
  class Platform_Impl : public Platform_Base
  {
    /// visit all adapters (not addresses). windows serves net info per adapter unlink posix which
    /// gives a list of all distinct addresses.
    template <typename Visit_t>
    void
    iter_adapters(Visit_t&& visit) const
    {
      constexpr auto flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST
          | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS
          | GAA_FLAG_INCLUDE_ALL_INTERFACES;

      ULONG sz{};
      GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, nullptr, &sz);
      auto* ptr = new uint8_t[sz];
      auto* addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(ptr);

      if (auto err = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, addrs, &sz);
          err != ERROR_SUCCESS)
        throw llarp::win32::error{err, "GetAdaptersAddresses()"};

      for (auto* addr = addrs; addr and addr->Next; addr = addr->Next)
        visit(addr);

      delete[] ptr;
    }

    template <typename adapter_t>
    bool
    adapter_has_ip(adapter_t* a, ipaddr_t ip) const
    {
      for (auto* addr = a->FirstUnicastAddress; addr and addr->Next; addr = addr->Next)
      {
        SockAddr saddr{*addr->Address.lpSockaddr};
        if (saddr.getIP() == ip)
          return true;
      }
      return false;
    }

    template <typename adapter_t>
    bool
    adapter_has_fam(adapter_t* a, int af) const
    {
      for (auto* addr = a->FirstUnicastAddress; addr and addr->Next; addr = addr->Next)
      {
        SockAddr saddr{*addr->Address.lpSockaddr};
        if (saddr.Family() == af)
          return true;
      }
      return false;
    }

   public:
    std::optional<int>
    GetInterfaceIndex(ipaddr_t ip) const override
    {
      std::optional<int> found;
      iter_adapters([&found, ip, this](auto* adapter) {
        if (found)
          return;
        if (adapter_has_ip(adapter, ip))
          found = adapter->IfIndex;
      });
      return found;
    }

    std::optional<llarp::SockAddr>
    GetInterfaceAddr(std::string_view name, int af) const override
    {
      std::optional<SockAddr> found;
      iter_adapters([name = std::string{name}, af, &found, this](auto* a) {
        if (found)
          return;
        if (std::string{a->AdapterName} != name)
          return;

        if (adapter_has_fam(a, af))
          found = SockAddr{*a->FirstUnicastAddress->Address.lpSockaddr};
      });
      return found;
    }

    std::optional<SockAddr>
    AllInterfaces(SockAddr fallback) const override
    {
      // windows seems to not give a shit about source address
      return fallback.isIPv6() ? SockAddr{"[::]"} : SockAddr{"0.0.0.0"};
    }

    std::optional<std::string>
    FindFreeTun() const override
    {
      // TODO: implement me ?
      return std::nullopt;
    }

    std::optional<std::string>
    GetBestNetIF(int) const override
    {
      // TODO: implement me ?
      return std::nullopt;
    }

    std::optional<IPRange>
    FindFreeRange() const override
    {
      std::list<IPRange> currentRanges;
      iter_adapters([&currentRanges](auto* i) {
        for (auto* addr = i->FirstUnicastAddress; addr and addr->Next; addr = addr->Next)
        {
          SockAddr saddr{*addr->Address.lpSockaddr};
          currentRanges.emplace_back(
              saddr.asIPv6(),
              ipaddr_netmask_bits(addr->OnLinkPrefixLength, addr->Address.lpSockaddr->sa_family));
        }
      });

      auto ownsRange = [&currentRanges](const IPRange& range) -> bool {
        for (const auto& ownRange : currentRanges)
        {
          if (ownRange * range)
            return true;
        }
        return false;
      };
      // generate possible ranges to in order of attempts
      std::list<IPRange> possibleRanges;
      for (byte_t oct = 16; oct < 32; ++oct)
      {
        possibleRanges.emplace_back(IPRange::FromIPv4(172, oct, 0, 1, 16));
      }
      for (byte_t oct = 0; oct < 255; ++oct)
      {
        possibleRanges.emplace_back(IPRange::FromIPv4(10, oct, 0, 1, 16));
      }
      for (byte_t oct = 0; oct < 255; ++oct)
      {
        possibleRanges.emplace_back(IPRange::FromIPv4(192, 168, oct, 1, 24));
      }
      // for each possible range pick the first one we don't own
      for (const auto& range : possibleRanges)
      {
        if (not ownsRange(range))
          return range;
      }
      return std::nullopt;
    }
    std::string
    LoopbackInterfaceName() const override
    {
      // todo: implement me? does windows even have a loopback?
      return "";
    }
    bool
    HasInterfaceAddress(ipaddr_t ip) const override
    {
      return GetInterfaceIndex(ip) != std::nullopt;
    }

    std::vector<InterfaceInfo>
    AllNetworkInterfaces() const override
    {
      std::vector<InterfaceInfo> all;
      iter_adapters([&all](auto* a) {
        auto& cur = all.emplace_back();
        cur.index = a->IfIndex;
        cur.name = a->AdapterName;
        for (auto* addr = a->FirstUnicastAddress; addr and addr->Next; addr = addr->Next)
        {
          SockAddr saddr{*addr->Address.lpSockaddr};
          cur.addrs.emplace_back(
              saddr.asIPv6(),
              ipaddr_netmask_bits(addr->OnLinkPrefixLength, addr->Address.lpSockaddr->sa_family));
        }
      });
      return all;
    }
  };

#else

  class Platform_Impl : public Platform
  {
    template <typename Visit_t>
    void
    iter_all(Visit_t&& visit) const
    {
      ifaddrs* addrs{nullptr};
      if (getifaddrs(&addrs))
        throw std::runtime_error{fmt::format("getifaddrs(): {}", strerror(errno))};

      for (auto next = addrs; next and next->ifa_next; next = next->ifa_next)
        visit(next);

      freeifaddrs(addrs);
    }

   public:
    std::string
    LoopbackInterfaceName() const override
    {
      std::string ifname;
      iter_all([this, &ifname](auto i) {
        if (i and i->ifa_addr and i->ifa_addr->sa_family == AF_INET)
        {
          const SockAddr addr{*i->ifa_addr};
          if (IsLoopbackAddress(addr.getIP()))
          {
            ifname = i->ifa_name;
          }
        }
      });
      if (ifname.empty())
        throw std::runtime_error{"we have no ipv4 loopback interface for some ungodly reason"};
      return ifname;
    }

    std::optional<std::string>
    GetBestNetIF(int af) const override
    {
      std::optional<std::string> found;
      iter_all([this, &found, af](auto i) {
        if (found)
          return;
        if (i and i->ifa_addr and i->ifa_addr->sa_family == af)
        {
          if (not IsBogon(*i->ifa_addr))
          {
            found = i->ifa_name;
          }
        }
      });

      return found;
    }

    std::optional<IPRange>
    FindFreeRange() const override
    {
      std::list<IPRange> currentRanges;
      iter_all([&currentRanges](auto i) {
        if (i and i->ifa_addr and i->ifa_addr->sa_family == AF_INET)
        {
          ipv4addr_t addr{reinterpret_cast<sockaddr_in*>(i->ifa_addr)->sin_addr.s_addr};
          ipv4addr_t mask{reinterpret_cast<sockaddr_in*>(i->ifa_netmask)->sin_addr.s_addr};
          currentRanges.emplace_back(IPRange::FromIPv4(addr, mask));
        }
      });

      auto ownsRange = [&currentRanges](const IPRange& range) -> bool {
        for (const auto& ownRange : currentRanges)
        {
          if (ownRange * range)
            return true;
        }
        return false;
      };
      // generate possible ranges to in order of attempts
      std::list<IPRange> possibleRanges;
      for (byte_t oct = 16; oct < 32; ++oct)
      {
        possibleRanges.emplace_back(IPRange::FromIPv4(172, oct, 0, 1, 16));
      }
      for (byte_t oct = 0; oct < 255; ++oct)
      {
        possibleRanges.emplace_back(IPRange::FromIPv4(10, oct, 0, 1, 16));
      }
      for (byte_t oct = 0; oct < 255; ++oct)
      {
        possibleRanges.emplace_back(IPRange::FromIPv4(192, 168, oct, 1, 24));
      }
      // for each possible range pick the first one we don't own
      for (const auto& range : possibleRanges)
      {
        if (not ownsRange(range))
          return range;
      }
      return std::nullopt;
    }

    std::optional<int> GetInterfaceIndex(ipaddr_t) const override
    {
      // todo: implement me
      return std::nullopt;
    }

    std::optional<std::string>
    FindFreeTun() const override
    {
      int num = 0;
      while (num < 255)
      {
        std::string ifname = fmt::format("lokitun{}", num);
        if (GetInterfaceAddr(ifname, AF_INET) == std::nullopt)
          return ifname;
        num++;
      }
      return std::nullopt;
    }

    std::optional<SockAddr>
    GetInterfaceAddr(std::string_view ifname, int af) const override
    {
      std::optional<SockAddr> addr;
      iter_all([&addr, af, ifname = std::string{ifname}](auto i) {
        if (addr)
          return;
        if (i and i->ifa_addr and i->ifa_addr->sa_family == af and i->ifa_name == ifname)
          addr = llarp::SockAddr{*i->ifa_addr};
      });
      return addr;
    }

    std::optional<SockAddr>
    AllInterfaces(SockAddr fallback) const override
    {
      std::optional<SockAddr> found;
      iter_all([fallback, &found](auto i) {
        if (found)
          return;
        if (i == nullptr or i->ifa_addr == nullptr)
          return;
        if (i->ifa_addr->sa_family != fallback.Family())
          return;
        SockAddr addr{*i->ifa_addr};
        if (addr == fallback)
          found = addr;
      });

      // 0.0.0.0 is used in our compat shim as our public ip so we check for that special case
      const auto zero = IPRange::FromIPv4(0, 0, 0, 0, 8);
      // when we cannot find an address but we are looking for 0.0.0.0 just default to the old
      // style
      if (not found and (fallback.isIPv4() and zero.Contains(fallback.asIPv4())))
        found = Wildcard(fallback.Family());
      return found;
    }

    bool
    HasInterfaceAddress(ipaddr_t ip) const override
    {
      bool found{false};
      iter_all([&found, ip](auto i) {
        if (found)
          return;

        if (not(i and i->ifa_addr))
          return;
        const SockAddr addr{*i->ifa_addr};
        found = addr.getIP() == ip;
      });
      return found;
    }
    std::vector<InterfaceInfo>
    AllNetworkInterfaces() const override
    {
      std::unordered_map<std::string, InterfaceInfo> ifmap;
      iter_all([&ifmap](auto* i) {
        if (i == nullptr or i->ifa_addr == nullptr)
          return;

        const auto fam = i->ifa_addr->sa_family;
        if (fam != AF_INET and fam != AF_INET6)
          return;

        auto& ent = ifmap[i->ifa_name];
        if (ent.name.empty())
        {
          ent.name = i->ifa_name;
          ent.index = if_nametoindex(i->ifa_name);
        }
        SockAddr addr{*i->ifa_addr};
        SockAddr mask{*i->ifa_netmask};
        ent.addrs.emplace_back(addr.asIPv6(), mask.asIPv6());
      });
      std::vector<InterfaceInfo> all;
      for (auto& [name, ent] : ifmap)
        all.emplace_back(std::move(ent));
      return all;
    }
  };
#endif

  const Platform_Impl g_plat{};

  const Platform*
  Platform::Default_ptr()
  {
    return &g_plat;
  }
}  // namespace llarp::net
