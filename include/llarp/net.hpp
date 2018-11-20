#ifndef LLARP_NET_HPP
#define LLARP_NET_HPP
#include <llarp/address_info.hpp>
#include <llarp/net.h>
#include <functional>
#include <iostream>
#include "logger.hpp"
#include "mem.hpp"
#include <llarp/string_view.hpp>
#include <vector>

#include <stdlib.h>  // for itoa

// for addrinfo
#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
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
  // clang-format off

  struct huint32_t
  {
    uint32_t h;

    constexpr huint32_t
    operator &(huint32_t x) const { return huint32_t{uint32_t(h & x.h)}; }
    constexpr huint32_t
    operator |(huint32_t x) const { return huint32_t{uint32_t(h | x.h)}; }
    constexpr huint32_t
    operator ^(huint32_t x) const { return huint32_t{uint32_t(h ^ x.h)}; }

    constexpr huint32_t
    operator ~() const { return huint32_t{uint32_t(~h)}; }

    inline huint32_t operator ++() { ++h; return *this; }
    inline huint32_t operator --() { --h; return *this; }

    constexpr bool operator <(huint32_t x) const { return h < x.h; }
    constexpr bool operator ==(huint32_t x) const { return h == x.h; }

    friend std::ostream&
    operator<<(std::ostream& out, const huint32_t& a)
    {
      uint32_t n = htonl(a.h);
      char tmp[INET_ADDRSTRLEN]   = {0};
      if(inet_ntop(AF_INET, (void*)&n, tmp, sizeof(tmp)))
      {
        out << tmp;
      }
      return out;
    }

    struct Hash
    {
      inline size_t
      operator ()(huint32_t x) const
      {
        return std::hash< uint32_t >{}(x.h);
      }
    };
  };

  struct nuint32_t
  {
    uint32_t n;

    constexpr nuint32_t
    operator &(nuint32_t x) const { return nuint32_t{uint32_t(n & x.n)}; }
    constexpr nuint32_t
    operator |(nuint32_t x) const { return nuint32_t{uint32_t(n | x.n)}; }
    constexpr nuint32_t
    operator ^(nuint32_t x) const { return nuint32_t{uint32_t(n ^ x.n)}; }

    constexpr nuint32_t
    operator ~() const { return nuint32_t{uint32_t(~n)}; }

    inline nuint32_t operator ++() { ++n; return *this; }
    inline nuint32_t operator --() { --n; return *this; }

    constexpr bool operator <(nuint32_t x) const { return n < x.n; }
    constexpr bool operator ==(nuint32_t x) const { return n == x.n; }

    friend std::ostream&
    operator<<(std::ostream& out, const nuint32_t& a)
    {
      char tmp[INET_ADDRSTRLEN]   = {0};
      if(inet_ntop(AF_INET, (void*)&a.n, tmp, sizeof(tmp)))
      {
        out << tmp;
      }
      return out;
    }

    struct Hash
    {
      inline size_t
      operator ()(nuint32_t x) const
      {
        return std::hash< uint32_t >{}(x.n);
      }
    };
  };

  struct huint16_t
  {
    uint16_t h;

    constexpr huint16_t
    operator &(huint16_t x) const { return huint16_t{uint16_t(h & x.h)}; }
    constexpr huint16_t
    operator |(huint16_t x) const { return huint16_t{uint16_t(h | x.h)}; }
    constexpr huint16_t
    operator ~() const { return huint16_t{uint16_t(~h)}; }

    inline huint16_t operator ++() { ++h; return *this; }
    inline huint16_t operator --() { --h; return *this; }

    constexpr bool operator <(huint16_t x) const { return h < x.h; }
    constexpr bool operator ==(huint16_t x) const { return h == x.h; }

    friend std::ostream&
    operator<<(std::ostream& out, const huint16_t& a)
    {
      return out << a.h;
    }

    struct Hash
    {
      inline size_t
      operator ()(huint16_t x) const
      {
        return std::hash< uint16_t >{}(x.h);
      }
    };
  };

  struct nuint16_t
  {
    uint16_t n;

    constexpr nuint16_t
    operator &(nuint16_t x) const { return nuint16_t{uint16_t(n & x.n)}; }
    constexpr nuint16_t
    operator |(nuint16_t x) const { return nuint16_t{uint16_t(n | x.n)}; }
    constexpr nuint16_t
    operator ~() const { return nuint16_t{uint16_t(~n)}; }

    inline nuint16_t operator ++() { ++n; return *this; }
    inline nuint16_t operator --() { --n; return *this; }

    constexpr bool operator <(nuint16_t x) const { return n < x.n; }
    constexpr bool operator ==(nuint16_t x) const { return n == x.n; }

    friend std::ostream&
    operator<<(std::ostream& out, const nuint16_t& a)
    {
      return out << ntohs(a.n);
    }

    struct Hash
    {
      inline size_t
      operator ()(nuint16_t x) const
      {
        return std::hash< uint16_t >{}(x.n);
      }
    };
  };

  // clang-format on

  static inline nuint32_t
  xhtonl(huint32_t x)
  {
    return nuint32_t{htonl(x.h)};
  }

  static inline huint32_t
  xntohl(nuint32_t x)
  {
    return huint32_t{ntohl(x.n)};
  }

  static inline nuint16_t
  xhtons(huint16_t x)
  {
    return nuint16_t{htons(x.h)};
  }

  static inline huint16_t
  xntohs(nuint16_t x)
  {
    return huint16_t{ntohs(x.n)};
  }

  struct IPRange
  {
    huint32_t addr;
    huint32_t netmask_bits;

    /// return true if ip is contained in this ip range
    bool
    Contains(const huint32_t& ip) const
    {
      // TODO: do this "better"
      return (addr & netmask_bits) == (ip & netmask_bits);
    }

    friend std::ostream&
    operator<<(std::ostream& out, const IPRange& a)
    {
      char strbuf[32] = {0};
      char netbuf[32] = {0};
      inet_ntop(AF_INET, (void*)&a.addr, strbuf, sizeof(strbuf));
      inet_ntop(AF_INET, (void*)&a.netmask_bits, netbuf, sizeof(netbuf));
      out << std::string(strbuf) + "/" + std::string(netbuf);
      return out;
    }
  };

  /// get a netmask with the higest numset bits set
  constexpr uint32_t
  __netmask_ipv4_bits(uint32_t numset)
  {
    return (32 - numset)
        ? (1 << (32 - (numset + 1))) | __netmask_ipv4_bits(numset + 1)
        : 0;
  }

  /// get an ipv4 netmask given some /N range
  constexpr huint32_t
  netmask_ipv4_bits(uint32_t num)
  {
    return huint32_t{__netmask_ipv4_bits(32 - num)};
  }

  constexpr huint32_t
  ipaddr_ipv4_bits(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
  {
    return huint32_t{(a) | (b << 8) | (c << 16) | (d << 24)};
  }

  constexpr IPRange
  iprange_ipv4(byte_t a, byte_t b, byte_t c, byte_t d, byte_t mask)
  {
    return IPRange{ipaddr_ipv4_bits(a, b, c, d), netmask_ipv4_bits(mask)};
  }

  bool
  IsIPv4Bogon(const huint32_t& addr);

  constexpr bool
  ipv6_is_siit(const in6_addr& addr)
  {
    return addr.s6_addr[11] == 0xff && addr.s6_addr[10] == 0xff
        && addr.s6_addr[9] == 0 && addr.s6_addr[8] == 0 && addr.s6_addr[7] == 0
        && addr.s6_addr[6] == 0 && addr.s6_addr[5] == 0 && addr.s6_addr[4] == 0
        && addr.s6_addr[3] == 0 && addr.s6_addr[2] == 0 && addr.s6_addr[1] == 0
        && addr.s6_addr[0] == 0;
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
  findFreePrivateRange();

  /// look at adapter names and find a free one
  std::string
  findFreeLokiTunIfName();

  /// get network interface address for network interface with ifname
  bool
  GetIFAddr(const std::string& ifname, Addr& addr, int af = AF_INET);

}  // namespace llarp

#include <llarp/net_inaddr.hpp>
#include <llarp/net_addr.hpp>

#endif
