#pragma once

#include "ip.hpp"
#include "net_int.hpp"

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

  // IPv4 mapped address live at ::ffff:0:0/96
  constexpr std::array<uint8_t, 12> ipv4_map_prefix{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};

  namespace net
  {
    inline auto
    ipaddr_netmask_bits(uint32_t bits, int af)
    {
      if (af == AF_INET6)
        return netmask_ipv6_bits(bits);
      return ExpandV4(netmask_ipv4_bits(bits));
    };
  }  // namespace net
}  // namespace llarp
