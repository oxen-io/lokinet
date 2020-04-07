#ifndef LLARP_NET_HPP
#define LLARP_NET_HPP

#include <net/uint128.hpp>
#include <net/address_info.hpp>
#include <net/net_int.hpp>
#include <net/net.h>
#include <util/mem.hpp>
#include <util/string_view.hpp>
#include <util/bits.hpp>

#include <functional>
#include <cstdlib>  // for itoa
#include <vector>

// for addrinfo
#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>
#define inet_aton(x, y) inet_pton(AF_INET, x, y)
#endif

bool
operator==(const sockaddr& a, const sockaddr& b);

bool
operator==(const sockaddr_in& a, const sockaddr_in& b);

bool
operator==(const sockaddr_in6& a, const sockaddr_in6& b);

bool
operator<(const sockaddr_in6& a, const sockaddr_in6& b);

bool
operator<(const in6_addr& a, const in6_addr& b);

bool
operator==(const in6_addr& a, const in6_addr& b);

struct privatesInUse
{
  // true if used by real NICs on start
  // false if not used, and means we could potentially use it if needed
  bool ten;       // 16m ips
  bool oneSeven;  // 1m  ips
  bool oneNine;   // 65k ips
};

struct privatesInUse
llarp_getPrivateIfs();

namespace llarp
{
  struct IPRange
  {
    using Addr_t = huint128_t;
    huint128_t addr = {0};
    huint128_t netmask_bits = {0};

    /// return true if ip is contained in this ip range
    bool
    Contains(const Addr_t& ip) const
    {
      return (addr & netmask_bits) == (ip & netmask_bits);
    }

    bool
    ContainsV4(const huint32_t& ip) const;

    friend std::ostream&
    operator<<(std::ostream& out, const IPRange& a)
    {
      return out << a.ToString();
    }

    /// get the highest address on this range
    huint128_t
    HighestAddr() const
    {
      return (addr & netmask_bits) + (huint128_t{1} << (128 - bits::count_bits_128(netmask_bits.h)))
          - huint128_t{1};
    }

    bool
    operator<(const IPRange& other) const
    {
      return (this->addr & this->netmask_bits) < (other.addr & other.netmask_bits)
          || this->netmask_bits < other.netmask_bits;
    }

    std::string
    ToString() const;

    bool
    FromString(std::string str);
  };

  huint128_t
  ExpandV4(huint32_t x);

  /// get a netmask with the higest numset bits set
  constexpr huint128_t
  __netmask_ipv6_bits(uint32_t numset)
  {
    return (128 - numset) ? (huint128_t{1} << numset) | __netmask_ipv6_bits(numset + 1)
                          : huint128_t{0};
  }

  constexpr huint128_t
  netmask_ipv6_bits(uint32_t numset)
  {
    return __netmask_ipv6_bits(128 - numset);
  }

  /// get a netmask with the higest numset bits set
  constexpr uint32_t
  __netmask_ipv4_bits(uint32_t numset)
  {
    return (32 - numset) ? (1 << numset) | __netmask_ipv4_bits(numset + 1) : 0;
  }

  /// get a netmask given some /N range
  constexpr huint32_t
  netmask_ipv4_bits(uint32_t num)
  {
    return huint32_t{__netmask_ipv4_bits(32 - num)};
  }

  constexpr huint32_t
  ipaddr_ipv4_bits(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
  {
    return huint32_t{(d) | (c << 8) | (b << 16) | (a << 24)};
  }

  IPRange
  iprange_ipv4(byte_t a, byte_t b, byte_t c, byte_t d, byte_t mask);

  bool
  IsIPv4Bogon(const huint32_t& addr);

  constexpr bool
  ipv6_is_siit(const in6_addr& addr)
  {
    return addr.s6_addr[11] == 0xff && addr.s6_addr[10] == 0xff && addr.s6_addr[9] == 0
        && addr.s6_addr[8] == 0 && addr.s6_addr[7] == 0 && addr.s6_addr[6] == 0
        && addr.s6_addr[5] == 0 && addr.s6_addr[4] == 0 && addr.s6_addr[3] == 0
        && addr.s6_addr[2] == 0 && addr.s6_addr[1] == 0 && addr.s6_addr[0] == 0;
  }

  bool
  IsBogon(const in6_addr& addr);

  bool
  IsBogonRange(const in6_addr& host, const in6_addr& mask);

  struct Addr;  // fwd declr

  bool
  AllInterfaces(int af, Addr& addr);

  /// get first network interface with public address
  bool
  GetBestNetIF(std::string& ifname, int af = AF_INET);

  /// look at adapter ranges and find a free one
  std::string
  FindFreeRange();

  /// look at adapter names and find a free one
  std::string
  FindFreeTun();

  /// get network interface address for network interface with ifname
  bool
  GetIFAddr(const std::string& ifname, Addr& addr, int af = AF_INET);

}  // namespace llarp

#include <net/net_addr.hpp>

#endif
