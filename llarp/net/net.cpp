#include <net/net.hpp>

#include <net/net_if.hpp>
#include <stdexcept>

#ifdef ANDROID
#include <android/ifaddrs.h>
#endif

#ifndef _WIN32
#include <arpa/inet.h>
#ifndef ANDROID
#include <ifaddrs.h>
#endif
#endif

#include <net/ip.hpp>
#include <util/logging/logger.hpp>
#include <util/str.hpp>

#include <cstdio>

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
      if (llarp::StrEq(i->ifa_name, ifname) && i->ifa_addr->sa_family == af)
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
          /* TODO: why the fuck does'n this work?
          // llarp::SockAddr a(i->ifa_addr);
          // llarp::IpAddress ip(a);

          if (!ip.isBogon())
          {
            ifname = i->ifa_name;
            found = true;
          }
          */
          throw std::runtime_error("WTF");
        }
      }
    });
    return found;
  }

  // TODO: ipv6?
  std::optional<std::string>
  FindFreeRange()
  {
    std::vector<IPRange> currentRanges;
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
          currentRanges.emplace_back(IPRange{net::IPPacket::ExpandV4(xntohl(ifaddr)),
                                             net::IPPacket::ExpandV4(xntohl(ifmask))});
      }
    });
    // try 10.x.0.0/16
    byte_t oct = 0;
    while (oct < 255)
    {
      const huint32_t loaddr = ipaddr_ipv4_bits(10, oct, 0, 1);
      const huint32_t hiaddr = ipaddr_ipv4_bits(10, oct, 255, 255);
      bool hit = false;
      for (const auto& range : currentRanges)
      {
        hit = hit || range.ContainsV4(loaddr) || range.ContainsV4(hiaddr);
      }
      if (!hit)
        return loaddr.ToString() + "/16";
      ++oct;
    }
    // try 192.168.x.0/24
    oct = 0;
    while (oct < 255)
    {
      const huint32_t loaddr = ipaddr_ipv4_bits(192, 168, oct, 1);
      const huint32_t hiaddr = ipaddr_ipv4_bits(192, 168, oct, 255);
      bool hit = false;
      for (const auto& range : currentRanges)
      {
        hit = hit || range.ContainsV4(loaddr) || range.ContainsV4(hiaddr);
      }
      if (!hit)
        return loaddr.ToString() + "/24";
    }
    // try 172.16.x.0/24
    oct = 0;
    while (oct < 255)
    {
      const huint32_t loaddr = ipaddr_ipv4_bits(172, 16, oct, 1);
      const huint32_t hiaddr = ipaddr_ipv4_bits(172, 16, oct, 255);
      bool hit = false;
      for (const auto& range : currentRanges)
      {
        hit = hit || range.ContainsV4(loaddr) || range.ContainsV4(hiaddr);
      }
      if (!hit)
        return loaddr.ToString() + "/24";
      ++oct;
    }
    return std::nullopt;
  }

  std::optional<std::string>
  FindFreeTun()
  {
    int num = 0;
    while (num < 255)
    {
      std::stringstream ifname_ss;
      ifname_ss << "lokitun" << num;
      std::string iftestname = ifname_ss.str();
      bool found = llarp_getifaddr(iftestname.c_str(), AF_INET, nullptr);
      if (!found)
      {
        return iftestname;
      }
      num++;
    }
    return std::nullopt;
  }

  std::optional<IpAddress>
  GetIFAddr(const std::string& ifname, int af)
  {
    sockaddr_storage s;
    sockaddr* sptr = (sockaddr*)&s;
    if (!llarp_getifaddr(ifname.c_str(), af, sptr))
      return std::nullopt;
    // TODO: why the fuck does this not compile?
    // llarp::SockAddr saddr = SockAddr(*sptr);
    // return llarp::IpAddress(saddr);
    throw std::runtime_error("WTF");
  }

  bool
  AllInterfaces(int af, IpAddress& result)
  {
    if (af == AF_INET)
    {
      sockaddr_in addr;
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = htonl(INADDR_ANY);
      addr.sin_port = htons(0);
      // TODO: why the fuck doesn't this work?
      // SockAddr saddr = SockAddr(addr);
      // result = IpAddress(saddr);
      throw std::runtime_error("WTF");
      return true;
    }
    if (af == AF_INET6)
    {
      throw std::runtime_error("Fix me: IPv6 not supported yet");
      /*
      sockaddr_in6 addr6;
      addr6.sin6_family = AF_INET6;
      addr6.sin6_port = htons(0);
      addr6.sin6_addr = IN6ADDR_ANY_INIT;
      result = IpAddress(SockAddr(addr6));
      return true;
      */
    }

    // TODO: implement sockaddr_ll

    return false;
  }

  bool
  IsBogon(const in6_addr& addr)
  {
#if defined(TESTNET)
    (void)addr;
    return false;
#else
    if (!ipv6_is_siit(addr))
    {
      static in6_addr zero = {};
      if (addr == zero)
        return true;
      return false;
    }
    return IsIPv4Bogon(
        ipaddr_ipv4_bits(addr.s6_addr[12], addr.s6_addr[13], addr.s6_addr[14], addr.s6_addr[15]));
#endif
  }

  bool
  IsBogonRange(const in6_addr& host, const in6_addr&)
  {
    // TODO: implement me
    return IsBogon(host);
  }

  bool
  IPRange::ContainsV4(const huint32_t& ip) const
  {
    return Contains(net::IPPacket::ExpandV4(ip));
  }

  bool
  IPRange::FromString(std::string str)
  {
    const auto colinpos = str.find(":");
    const auto slashpos = str.find("/");
    std::string bitsstr;
    if (slashpos != std::string::npos)
    {
      bitsstr = str.substr(slashpos + 1);
      str = str.substr(0, slashpos);
    }
    if (colinpos == std::string::npos)
    {
      huint32_t ip;
      if (!ip.FromString(str))
        return false;
      addr = net::IPPacket::ExpandV4(ip);
      if (!bitsstr.empty())
      {
        auto bits = atoi(bitsstr.c_str());
        if (bits < 0 || bits > 32)
          return false;
        netmask_bits = netmask_ipv6_bits(96 + bits);
      }
      else
        netmask_bits = netmask_ipv6_bits(128);
    }
    else
    {
      if (!addr.FromString(str))
        return false;
      if (!bitsstr.empty())
      {
        auto bits = atoi(bitsstr.c_str());
        if (bits < 0 || bits > 128)
          return false;
        netmask_bits = netmask_ipv6_bits(bits);
      }
      else
      {
        netmask_bits = netmask_ipv6_bits(128);
      }
    }
    return true;
  }

  std::string
  IPRange::ToString() const
  {
    char buf[INET6_ADDRSTRLEN + 1] = {0};
    std::string str;
    in6_addr inaddr = {};
    size_t numset = 0;
    uint128_t bits = netmask_bits.h;
    while (bits)
    {
      if (bits & 1)
        numset++;
      bits >>= 1;
    }
    str += inet_ntop(AF_INET6, &inaddr, buf, sizeof(buf));
    return str + "/" + std::to_string(numset);
  }

  IPRange
  iprange_ipv4(byte_t a, byte_t b, byte_t c, byte_t d, byte_t mask)
  {
    return IPRange{net::IPPacket::ExpandV4(ipaddr_ipv4_bits(a, b, c, d)),
                   netmask_ipv6_bits(mask + 96)};
  }

  bool
  IsIPv4Bogon(const huint32_t& addr)
  {
    static std::vector<IPRange> bogonRanges = {iprange_ipv4(0, 0, 0, 0, 8),
                                               iprange_ipv4(10, 0, 0, 0, 8),
                                               iprange_ipv4(21, 0, 0, 0, 8),
                                               iprange_ipv4(100, 64, 0, 0, 10),
                                               iprange_ipv4(127, 0, 0, 0, 8),
                                               iprange_ipv4(169, 254, 0, 0, 8),
                                               iprange_ipv4(172, 16, 0, 0, 12),
                                               iprange_ipv4(192, 0, 0, 0, 24),
                                               iprange_ipv4(192, 0, 2, 0, 24),
                                               iprange_ipv4(192, 88, 99, 0, 24),
                                               iprange_ipv4(192, 168, 0, 0, 16),
                                               iprange_ipv4(198, 18, 0, 0, 15),
                                               iprange_ipv4(198, 51, 100, 0, 24),
                                               iprange_ipv4(203, 0, 113, 0, 24),
                                               iprange_ipv4(224, 0, 0, 0, 4),
                                               iprange_ipv4(240, 0, 0, 0, 4)};
    for (const auto& bogon : bogonRanges)
    {
      if (bogon.ContainsV4(addr))
      {
#if defined(TESTNET)
        return false;
#else
        return true;
#endif
      }
    }
    return false;
  }
}  // namespace llarp
