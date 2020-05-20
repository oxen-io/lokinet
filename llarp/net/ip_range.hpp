#pragma once
#include <ostream>
#include <net/ip.hpp>
#include <util/bits.hpp>
#include <util/types.hpp>
#include <string>

namespace llarp
{
  struct IPRange
  {
    using Addr_t = huint128_t;
    huint128_t addr = {0};
    huint128_t netmask_bits = {0};

    static constexpr IPRange
    FromIPv4(byte_t a, byte_t b, byte_t c, byte_t d, byte_t mask)
    {
      return IPRange{net::ExpandV4(ipaddr_ipv4_bits(a, b, c, d)),
                     netmask_ipv6_bits(mask + 96)};
    }

    /// return true if ip is contained in this ip range
    constexpr bool
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

}  // namespace llarp
