#include "net.hpp"

#include "net_if.hpp"
#include <stdexcept>

#ifdef ANDROID
#include <llarp/android/ifaddrs.h>
#endif

#ifndef _WIN32
#include <arpa/inet.h>
#ifndef ANDROID
#include <ifaddrs.h>
#endif
#endif

#include "ip.hpp"
#include "ip_range.hpp"
#include <llarp/util/logging.hpp>
#include <llarp/util/str.hpp>

#ifdef ANDROID
#include <llarp/android/ifaddrs.h>
#else
#ifndef _WIN32
#include <ifaddrs.h>
#endif
#endif

#include <cstdio>
#include <list>

bool
operator==(const sockaddr& a, const sockaddr& b)
{
  if (a.sa_family != b.sa_family)
    return false;
  switch (a.sa_family)
  {
    case AF_INET:
      return *((const sockaddr_in*)&a) == *((const sockaddr_in*)&b);
    case AF_INET6:
      return *((const sockaddr_in6*)&a) == *((const sockaddr_in6*)&b);
    default:
      return false;
  }
}

bool
operator<(const sockaddr_in6& a, const sockaddr_in6& b)
{
  return a.sin6_addr < b.sin6_addr || a.sin6_port < b.sin6_port;
}

bool
operator<(const in6_addr& a, const in6_addr& b)
{
  return memcmp(&a, &b, sizeof(in6_addr)) < 0;
}

bool
operator==(const in6_addr& a, const in6_addr& b)
{
  return memcmp(&a, &b, sizeof(in6_addr)) == 0;
}

bool
operator==(const sockaddr_in& a, const sockaddr_in& b)
{
  return a.sin_port == b.sin_port && a.sin_addr.s_addr == b.sin_addr.s_addr;
}

bool
operator==(const sockaddr_in6& a, const sockaddr_in6& b)
{
  return a.sin6_port == b.sin6_port && a.sin6_addr == b.sin6_addr;
}

#ifdef _WIN32
#include <assert.h>
#include <errno.h>
#include <iphlpapi.h>
#include <strsafe.h>

// current strategy: mingw 32-bit builds call an inlined version of the function
// microsoft c++ and mingw 64-bit builds call the normal function
#define DEFAULT_BUFFER_SIZE 15000

// in any case, we still need to implement some form of
// getifaddrs(3) with compatible semantics on NT...
// daemon.ini section [bind] will have something like
// [bind]
// Ethernet=1090
// inside, since that's what we use in windows to refer to
// network interfaces
struct llarp_nt_ifaddrs_t
{
  struct llarp_nt_ifaddrs_t* ifa_next; /* Pointer to the next structure.  */
  char* ifa_name;                      /* Name of this network interface.  */
  unsigned int ifa_flags;              /* Flags as from SIOCGIFFLAGS ioctl.  */
  struct sockaddr* ifa_addr;           /* Network address of this interface.  */
  struct sockaddr* ifa_netmask;        /* Netmask of this interface.  */
};

// internal struct
struct _llarp_nt_ifaddrs_t
{
  struct llarp_nt_ifaddrs_t _ifa;
  char _name[256];
  struct sockaddr_storage _addr;
  struct sockaddr_storage _netmask;
};

static inline void*
_llarp_nt_heap_alloc(const size_t n_bytes)
{
  /* Does not appear very safe with re-entrant calls on XP */
  return HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, n_bytes);
}

static inline void
_llarp_nt_heap_free(void* mem)
{
  HeapFree(GetProcessHeap(), 0, mem);
}
#define llarp_nt_new0(struct_type, n_structs) \
  ((struct_type*)malloc((size_t)sizeof(struct_type) * (size_t)(n_structs)))

int
llarp_nt_sockaddr_pton(const char* src, struct sockaddr* dst)
{
  struct addrinfo hints;
  struct addrinfo* result = nullptr;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_NUMERICHOST;
  const int status = getaddrinfo(src, nullptr, &hints, &result);
  if (!status)
  {
    memcpy(dst, result->ai_addr, result->ai_addrlen);
    freeaddrinfo(result);
    return 1;
  }
  return 0;
}

/* NB: IP_ADAPTER_INFO size varies size due to sizeof (time_t), the API assumes
 * 4-byte datatype whilst compiler uses an 8-byte datatype.  Size can be forced
 * with -D_USE_32BIT_TIME_T with side effects to everything else.
 *
 * Only supports IPv4 addressing similar to SIOCGIFCONF socket option.
 *
 * Interfaces that are not "operationally up" will return the address 0.0.0.0,
 * this includes adapters with static IP addresses but with disconnected cable.
 * This is documented under the GetIpAddrTable API.  Interface status can only
 * be determined by the address, a separate flag is introduced with the
 * GetAdapterAddresses API.
 *
 * The IPv4 loopback interface is not included.
 *
 * Available in Windows 2000 and Wine 1.0.
 */
static bool
_llarp_nt_getadaptersinfo(struct llarp_nt_ifaddrs_t** ifap)
{
  DWORD dwRet;
  ULONG ulOutBufLen = DEFAULT_BUFFER_SIZE;
  PIP_ADAPTER_INFO pAdapterInfo = nullptr;
  PIP_ADAPTER_INFO pAdapter = nullptr;

  /* loop to handle interfaces coming online causing a buffer overflow
   * between first call to list buffer length and second call to enumerate.
   */
  for (unsigned i = 3; i; i--)
  {
#ifdef DEBUG
    fprintf(stderr, "IP_ADAPTER_INFO buffer length %lu bytes.\n", ulOutBufLen);
#endif
    pAdapterInfo = (IP_ADAPTER_INFO*)_llarp_nt_heap_alloc(ulOutBufLen);
    dwRet = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
    if (ERROR_BUFFER_OVERFLOW == dwRet)
    {
      _llarp_nt_heap_free(pAdapterInfo);
      pAdapterInfo = nullptr;
    }
    else
    {
      break;
    }
  }

  switch (dwRet)
  {
    case ERROR_SUCCESS: /* NO_ERROR */
      break;
    case ERROR_BUFFER_OVERFLOW:
      errno = ENOBUFS;
      if (pAdapterInfo)
        _llarp_nt_heap_free(pAdapterInfo);
      return false;
    default:
      errno = dwRet;
#ifdef DEBUG
      fprintf(stderr, "system call failed: %lu\n", GetLastError());
#endif
      if (pAdapterInfo)
        _llarp_nt_heap_free(pAdapterInfo);
      return false;
  }

  /* count valid adapters */
  int n = 0, k = 0;
  for (pAdapter = pAdapterInfo; pAdapter; pAdapter = pAdapter->Next)
  {
    for (IP_ADDR_STRING* pIPAddr = &pAdapter->IpAddressList; pIPAddr; pIPAddr = pIPAddr->Next)
    {
      /* skip null adapters */
      if (strlen(pIPAddr->IpAddress.String) == 0)
        continue;
      ++n;
    }
  }

#ifdef DEBUG
  fprintf(stderr, "GetAdaptersInfo() discovered %d interfaces.\n", n);
#endif

  /* contiguous block for adapter list */
  struct _llarp_nt_ifaddrs_t* ifa = llarp_nt_new0(struct _llarp_nt_ifaddrs_t, n);
  struct _llarp_nt_ifaddrs_t* ift = ifa;
  int val = 0;
  /* now populate list */
  for (pAdapter = pAdapterInfo; pAdapter; pAdapter = pAdapter->Next)
  {
    for (IP_ADDR_STRING* pIPAddr = &pAdapter->IpAddressList; pIPAddr; pIPAddr = pIPAddr->Next)
    {
      /* skip null adapters */
      if (strlen(pIPAddr->IpAddress.String) == 0)
        continue;

      /* address */
      ift->_ifa.ifa_addr = (struct sockaddr*)&ift->_addr;
      val = llarp_nt_sockaddr_pton(pIPAddr->IpAddress.String, ift->_ifa.ifa_addr);
      assert(1 == val);

      /* name */
#ifdef DEBUG
      fprintf(stderr, "name:%s IPv4 index:%lu\n", pAdapter->AdapterName, pAdapter->Index);
#endif
      ift->_ifa.ifa_name = ift->_name;
      StringCchCopyN(ift->_ifa.ifa_name, 128, pAdapter->AdapterName, 128);

      /* flags: assume up, broadcast and multicast */
      ift->_ifa.ifa_flags = IFF_UP | IFF_BROADCAST | IFF_MULTICAST;
      if (pAdapter->Type == MIB_IF_TYPE_LOOPBACK)
        ift->_ifa.ifa_flags |= IFF_LOOPBACK;

      /* netmask */
      ift->_ifa.ifa_netmask = (sockaddr*)&ift->_netmask;
      val = llarp_nt_sockaddr_pton(pIPAddr->IpMask.String, ift->_ifa.ifa_netmask);
      assert(1 == val);

      /* next */
      if (k++ < (n - 1))
      {
        ift->_ifa.ifa_next = (struct llarp_nt_ifaddrs_t*)(ift + 1);
        ift = (struct _llarp_nt_ifaddrs_t*)(ift->_ifa.ifa_next);
      }
      else
      {
        ift->_ifa.ifa_next = nullptr;
      }
    }
  }

  if (pAdapterInfo)
    _llarp_nt_heap_free(pAdapterInfo);
  *ifap = (struct llarp_nt_ifaddrs_t*)ifa;
  return true;
}

// an implementation of if_nametoindex(3) based on GetAdapterIndex(2)
// with a fallback to GetAdaptersAddresses(2) commented out for now
// unless it becomes evident that the first codepath fails in certain
// edge cases?
static unsigned
_llarp_nt_nametoindex(const char* ifname)
{
  ULONG ifIndex;
  DWORD dwRet;
  char szAdapterName[256];

  if (!ifname)
    return 0;

  StringCchCopyN(szAdapterName, sizeof(szAdapterName), ifname, 256);
  dwRet = GetAdapterIndex((LPWSTR)szAdapterName, &ifIndex);

  if (!dwRet)
    return ifIndex;
  else
    return 0;
}

// the emulated getifaddrs(3) itself.
static bool
llarp_nt_getifaddrs(struct llarp_nt_ifaddrs_t** ifap)
{
  assert(nullptr != ifap);
#ifdef DEBUG
  fprintf(stderr, "llarp_nt_getifaddrs (ifap:%p error:%p)\n", (void*)ifap, (void*)errno);
#endif
  return _llarp_nt_getadaptersinfo(ifap);
}

static void
llarp_nt_freeifaddrs(struct llarp_nt_ifaddrs_t* ifa)
{
  if (!ifa)
    return;
  free(ifa);
}

// emulated if_nametoindex(3)
static unsigned
llarp_nt_if_nametoindex(const char* ifname)
{
  if (!ifname)
    return 0;
  return _llarp_nt_nametoindex(ifname);
}

// fix up names for win32
#define ifaddrs llarp_nt_ifaddrs_t
#define getifaddrs llarp_nt_getifaddrs
#define freeifaddrs llarp_nt_freeifaddrs
#define if_nametoindex llarp_nt_if_nametoindex
#endif

// jeff's original code
bool
llarp_getifaddr(const char* ifname, int af, struct sockaddr* addr)
{
  ifaddrs* ifa = nullptr;
  bool found = false;
  socklen_t sl = sizeof(sockaddr_in6);
  if (af == AF_INET)
    sl = sizeof(sockaddr_in);

#ifndef _WIN32
  if (getifaddrs(&ifa) == -1)
#else
  if (!strcmp(ifname, "lo") || !strcmp(ifname, "lo0"))
  {
    if (addr)
    {
      sockaddr_in* lo = (sockaddr_in*)addr;
      lo->sin_family = af;
      lo->sin_port = 0;
      inet_pton(af, "127.0.0.1", &lo->sin_addr);
    }
    return true;
  }
  if (!getifaddrs(&ifa))
#endif
    return false;
  ifaddrs* i = ifa;
  while (i)
  {
    if (i->ifa_addr)
    {
      // llarp::LogInfo(__FILE__, "scanning ", i->ifa_name, " af: ",
      // std::to_string(i->ifa_addr->sa_family));
      if (std::string_view{i->ifa_name} == std::string_view{ifname} && i->ifa_addr->sa_family == af)
      {
        // can't do this here
        // llarp::Addr a(*i->ifa_addr);
        // if(!a.isPrivate())
        //{
        // llarp::LogInfo(__FILE__, "found ", ifname, " af: ", af);
        if (addr)
        {
          memcpy(addr, i->ifa_addr, sl);
          if (af == AF_INET6)
          {
            // set scope id
            auto* ip6addr = (sockaddr_in6*)addr;
            ip6addr->sin6_scope_id = if_nametoindex(ifname);
            ip6addr->sin6_flowinfo = 0;
          }
        }
        found = true;
        break;
      }
      //}
    }
    i = i->ifa_next;
  }
  if (ifa)
    freeifaddrs(ifa);
  return found;
}

namespace llarp
{
  static void
  IterAllNetworkInterfaces(std::function<void(ifaddrs* const)> visit)
  {
    ifaddrs* ifa = nullptr;
#ifndef _WIN32
    if (getifaddrs(&ifa) == -1)
#else
    if (!getifaddrs(&ifa))
#endif
      return;

    ifaddrs* i = ifa;
    while (i)
    {
      visit(i);
      i = i->ifa_next;
    }

    if (ifa)
      freeifaddrs(ifa);
  }
  namespace net
  {
    std::string
    LoopbackInterfaceName()
    {
      const auto loopback = IPRange::FromIPv4(127, 0, 0, 0, 8);
      std::string ifname;
      IterAllNetworkInterfaces([&ifname, loopback](ifaddrs* const i) {
        if (i->ifa_addr and i->ifa_addr->sa_family == AF_INET)
        {
          llarp::nuint32_t addr{((sockaddr_in*)i->ifa_addr)->sin_addr.s_addr};
          if (loopback.Contains(xntohl(addr)))
          {
            ifname = i->ifa_name;
          }
        }
      });
      if (ifname.empty())
      {
        throw std::runtime_error(
            "we have no ipv4 loopback interface for some ungodly reason, yeah idk fam");
      }
      return ifname;
    }
  }  // namespace net

  bool
  GetBestNetIF(std::string& ifname, int af)
  {
    bool found = false;
    IterAllNetworkInterfaces([&](ifaddrs* i) {
      if (found)
        return;
      if (i->ifa_addr)
      {
        if (i->ifa_addr->sa_family == af)
        {
          llarp::SockAddr a(*i->ifa_addr);
          llarp::IpAddress ip(a);

          if (!ip.isBogon())
          {
            ifname = i->ifa_name;
            found = true;
          }
        }
      }
    });
    return found;
  }

  // TODO: ipv6?
  std::optional<IPRange>
  FindFreeRange()
  {
    std::list<IPRange> currentRanges;
    IterAllNetworkInterfaces([&](ifaddrs* i) {
      if (i && i->ifa_addr)
      {
        const auto fam = i->ifa_addr->sa_family;
        if (fam != AF_INET)
          return;
        auto* addr = (sockaddr_in*)i->ifa_addr;
        auto* mask = (sockaddr_in*)i->ifa_netmask;
        nuint32_t ifaddr{addr->sin_addr.s_addr};
        nuint32_t ifmask{mask->sin_addr.s_addr};
#ifdef _WIN32
        // do not delete, otherwise GCC will do horrible things to this lambda
        LogDebug("found ", ifaddr, " with mask ", ifmask);
#endif
        if (addr->sin_addr.s_addr)
          // skip unconfig'd adapters (windows passes these through the unix-y
          // wrapper)
          currentRanges.emplace_back(
              IPRange{net::ExpandV4(xntohl(ifaddr)), net::ExpandV4(xntohl(ifmask))});
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

  std::optional<std::string>
  FindFreeTun()
  {
    int num = 0;
    while (num < 255)
    {
      std::string iftestname = fmt::format("lokitun{}", num);
      bool found = llarp_getifaddr(iftestname.c_str(), AF_INET, nullptr);
      if (!found)
      {
        return iftestname;
      }
      num++;
    }
    return std::nullopt;
  }

  std::optional<SockAddr>
  GetInterfaceAddr(const std::string& ifname, int af)
  {
    sockaddr_storage s;
    sockaddr* sptr = (sockaddr*)&s;
    sptr->sa_family = af;
    if (!llarp_getifaddr(ifname.c_str(), af, sptr))
      return std::nullopt;
    return SockAddr{*sptr};
  }

  std::optional<huint128_t>
  GetInterfaceIPv6Address(std::string ifname)
  {
    sockaddr_storage s;
    sockaddr* sptr = (sockaddr*)&s;
    sptr->sa_family = AF_INET6;
    if (!llarp_getifaddr(ifname.c_str(), AF_INET6, sptr))
      return std::nullopt;
    llarp::SockAddr addr{*sptr};
    return addr.asIPv6();
  }

  namespace net
  {
    namespace
    {
      SockAddr
      All(int af)
      {
        if (af == AF_INET)
        {
          sockaddr_in addr{};
          addr.sin_family = AF_INET;
          addr.sin_addr.s_addr = htonl(INADDR_ANY);
          addr.sin_port = htons(0);
          return SockAddr{addr};
        }
        if (af == AF_INET6)
        {
          sockaddr_in6 addr6{};
          addr6.sin6_family = AF_INET6;
          addr6.sin6_port = htons(0);
          addr6.sin6_addr = IN6ADDR_ANY_INIT;
          return SockAddr{addr6};
        }
        throw std::invalid_argument{fmt::format("{} is not a valid address family", af)};
      }
    }  // namespace

    std::optional<SockAddr>
    AllInterfaces(SockAddr pub)
    {
      std::optional<SockAddr> found;
      IterAllNetworkInterfaces([pub, &found](auto* ifa) {
        if (found)
          return;
        if (auto ifa_addr = ifa->ifa_addr)
        {
          if (ifa_addr->sa_family != pub.Family())
            return;

          SockAddr addr{*ifa->ifa_addr};

          if (addr == pub)
            found = addr;
        }
      });

      // 0.0.0.0 is used in our compat shim as our public ip so we check for that special case
      const auto zero = IPRange::FromIPv4(0, 0, 0, 0, 8);
      // when we cannot find an address but we are looking for 0.0.0.0 just default to the old style
      if (not found and (pub.isIPv4() and zero.Contains(pub.asIPv4())))
        found = All(pub.Family());
      return found;
    }
  }  // namespace net

#if !defined(TESTNET)
  static constexpr std::array bogonRanges_v6 = {
      // zero
      IPRange{huint128_t{0}, netmask_ipv6_bits(128)},
      // loopback
      IPRange{huint128_t{1}, netmask_ipv6_bits(128)},
      // yggdrasil
      IPRange{huint128_t{uint128_t{0x0200'0000'0000'0000UL, 0UL}}, netmask_ipv6_bits(7)},
      // multicast
      IPRange{huint128_t{uint128_t{0xff00'0000'0000'0000UL, 0UL}}, netmask_ipv6_bits(8)},
      // local
      IPRange{huint128_t{uint128_t{0xfc00'0000'0000'0000UL, 0UL}}, netmask_ipv6_bits(8)},
      // local
      IPRange{huint128_t{uint128_t{0xf800'0000'0000'0000UL, 0UL}}, netmask_ipv6_bits(8)}};

  static constexpr std::array bogonRanges_v4 = {
      IPRange::FromIPv4(0, 0, 0, 0, 8),
      IPRange::FromIPv4(10, 0, 0, 0, 8),
      IPRange::FromIPv4(100, 64, 0, 0, 10),
      IPRange::FromIPv4(127, 0, 0, 0, 8),
      IPRange::FromIPv4(169, 254, 0, 0, 16),
      IPRange::FromIPv4(172, 16, 0, 0, 12),
      IPRange::FromIPv4(192, 0, 0, 0, 24),
      IPRange::FromIPv4(192, 0, 2, 0, 24),
      IPRange::FromIPv4(192, 88, 99, 0, 24),
      IPRange::FromIPv4(192, 168, 0, 0, 16),
      IPRange::FromIPv4(198, 18, 0, 0, 15),
      IPRange::FromIPv4(198, 51, 100, 0, 24),
      IPRange::FromIPv4(203, 0, 113, 0, 24),
      IPRange::FromIPv4(224, 0, 0, 0, 4),
      IPRange::FromIPv4(240, 0, 0, 0, 4)};

#endif

  bool
  IsBogon(const in6_addr& addr)
  {
#if defined(TESTNET)
    (void)addr;
    return false;
#else
    if (not ipv6_is_mapped_ipv4(addr))
    {
      const auto ip = net::In6ToHUInt(addr);
      for (const auto& range : bogonRanges_v6)
      {
        if (range.Contains(ip))
          return true;
      }
      return false;
    }
    return IsIPv4Bogon(
        ipaddr_ipv4_bits(addr.s6_addr[12], addr.s6_addr[13], addr.s6_addr[14], addr.s6_addr[15]));
#endif
  }

  bool
  IsBogon(const huint128_t ip)
  {
    const nuint128_t netIP{ntoh128(ip.h)};
    in6_addr addr{};
    std::copy_n((const uint8_t*)&netIP.n, 16, &addr.s6_addr[0]);
    return IsBogon(addr);
  }

  bool
  IsBogonRange(const in6_addr& host, const in6_addr&)
  {
    // TODO: implement me
    return IsBogon(host);
  }

#if !defined(TESTNET)
  bool
  IsIPv4Bogon(const huint32_t& addr)
  {
    for (const auto& bogon : bogonRanges_v4)
    {
      if (bogon.Contains(addr))
      {
        return true;
      }
    }
    return false;
  }
#else
  bool
  IsIPv4Bogon(const huint32_t&)
  {
    return false;
  }
#endif
  bool
  HasInterfaceAddress(std::variant<nuint32_t, nuint128_t> ip)
  {
    bool found{false};
    IterAllNetworkInterfaces([ip, &found](const auto* iface) {
      if (found or iface == nullptr)
        return;
      if (auto addr = iface->ifa_addr;
          addr and (addr->sa_family == AF_INET or addr->sa_family == AF_INET6))
      {
        found = SockAddr{*iface->ifa_addr}.getIP() == ip;
      }
    });
    return found;
  }

}  // namespace llarp
