#pragma once
#include <cstddef>
#include "net_int.hpp"

namespace llarp
{

  /// get a netmask given some /N range
  template <typename UInt_t>
  constexpr huint_t<UInt_t>
  netmask_bits(const int numset)
  {
    constexpr int max_bits = sizeof(UInt_t) * 8;
    if (numset > max_bits)
      throw std::invalid_argument{
          fmt::format("invalid netmask size requested: {} > {}", numset, max_bits)};
    huint_t<UInt_t> bits{};
    for (int idx = numset; idx > 0; --idx)
    {
      bits.h |= (UInt_t{1} << (max_bits - idx));
    }
    return bits;
  }

  constexpr huint128_t
  netmask_ipv6_bits(int numset)
  {
    return netmask_bits<uint128_t>(numset);
  }

  constexpr huint32_t
  netmask_ipv4_bits(int numset)
  {
    return netmask_bits<uint32_t>(numset);
  }

  constexpr huint32_t
  ipaddr_ipv4_bits(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
  {
    return huint32_t{(d) | (c << 8) | (b << 16) | (a << 24)};
  }

  // IPv4 mapped address live at ::ffff:0:0/96
  constexpr std::array<uint8_t, 12> ipv4_map_prefix{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};

  constexpr auto
  ipaddr_netmask_bits(int cidr, int af)
  {
    if (af == AF_INET)
      return net::ExpandV4(netmask_bits<uint32_t>(cidr));
    else
      return netmask_bits<uint128_t>(cidr);
  }

}  // namespace llarp
