#pragma once
#include <net/net_int.hpp>

namespace llarp
{
  /// get a netmask with the higest numset bits set
  constexpr huint128_t
  _netmask_ipv6_bits(uint32_t numset)
  {
    return (128 - numset) ? (huint128_t{1} << numset) | _netmask_ipv6_bits(numset + 1)
                          : huint128_t{0};
  }

  constexpr huint128_t
  netmask_ipv6_bits(uint32_t numset)
  {
    return _netmask_ipv6_bits(128 - numset);
  }

  /// get a netmask with the higest numset bits set
  constexpr uint32_t
  _netmask_ipv4_bits(uint32_t numset)
  {
    return (32 - numset) ? (1 << numset) | _netmask_ipv4_bits(numset + 1) : 0;
  }

  /// get a netmask given some /N range
  constexpr huint32_t
  netmask_ipv4_bits(uint32_t num)
  {
    return huint32_t{_netmask_ipv4_bits(32 - num)};
  }

  constexpr huint32_t
  ipaddr_ipv4_bits(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
  {
    return huint32_t{(d) | (c << 8) | (b << 16) | (a << 24)};
  }

  constexpr bool
  ipv6_is_siit(const in6_addr& addr)
  {
    return addr.s6_addr[11] == 0xff && addr.s6_addr[10] == 0xff && addr.s6_addr[9] == 0
        && addr.s6_addr[8] == 0 && addr.s6_addr[7] == 0 && addr.s6_addr[6] == 0
        && addr.s6_addr[5] == 0 && addr.s6_addr[4] == 0 && addr.s6_addr[3] == 0
        && addr.s6_addr[2] == 0 && addr.s6_addr[1] == 0 && addr.s6_addr[0] == 0;
  }
}  // namespace llarp
