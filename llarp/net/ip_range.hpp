#pragma once
#include <ostream>
#include "ip.hpp"
#include "net_bits.hpp"
#include <llarp/util/bits.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/types.hpp>
#include <string>

namespace llarp
{
  /// forward declare
  bool
  IsBogon(huint128_t ip);

  struct IPRange
  {
    using Addr_t = huint128_t;
    huint128_t addr = {0};
    huint128_t netmask_bits = {0};

    constexpr IPRange()
    {}
    constexpr IPRange(huint128_t address, huint128_t netmask)
        : addr{std::move(address)}, netmask_bits{std::move(netmask)}
    {}

    static constexpr IPRange
    FromIPv4(byte_t a, byte_t b, byte_t c, byte_t d, byte_t mask)
    {
      return IPRange{net::ExpandV4(ipaddr_ipv4_bits(a, b, c, d)), netmask_ipv6_bits(mask + 96)};
    }

    /// return true if this iprange is in the IPv4 mapping range for containing ipv4 addresses
    constexpr bool
    IsV4() const
    {
      constexpr auto ipv4_map = IPRange{huint128_t{0x0000'ffff'0000'0000UL}, netmask_ipv6_bits(96)};
      return ipv4_map.Contains(addr);
    }

    /// return true if we intersect with a bogon range
    bool
    BogonRange() const
    {
      // special case for 0.0.0.0/0
      if (IsV4() and netmask_bits == netmask_ipv6_bits(96))
        return false;
      // special case for ::/0
      if (netmask_bits == huint128_t{0})
        return false;
      return IsBogon(addr) or IsBogon(HighestAddr());
    }

    /// return the number of bits set in the hostmask
    constexpr int
    HostmaskBits() const
    {
      if (IsV4())
      {
        return bits::count_bits(net::TruncateV6(netmask_bits));
      }
      return bits::count_bits(netmask_bits);
    }

    /// return true if the other range is inside our range
    constexpr bool
    Contains(const IPRange& other) const
    {
      return Contains(other.addr) and Contains(other.HighestAddr());
    }

    /// return true if ip is contained in this ip range
    constexpr bool
    Contains(const Addr_t& ip) const
    {
      return (addr & netmask_bits) == (ip & netmask_bits);
    }

    /// return true if we are a ipv4 range and contains this ip
    constexpr bool
    Contains(const huint32_t& ip) const
    {
      if (not IsV4())
        return false;
      return Contains(net::ExpandV4(ip));
    }

    friend std::ostream&
    operator<<(std::ostream& out, const IPRange& a)
    {
      return out << a.ToString();
    }

    /// get the highest address on this range
    constexpr huint128_t
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

    bool
    operator==(const IPRange& other) const
    {
      return addr == other.addr and netmask_bits == other.netmask_bits;
    }

    std::string
    ToString() const
    {
      return BaseAddressString() + "/" + std::to_string(HostmaskBits());
    }

    std::string
    BaseAddressString() const;

    bool
    FromString(std::string str);

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    BDecode(llarp_buffer_t* buf);
  };

}  // namespace llarp

namespace std
{
  template <>
  struct hash<llarp::IPRange>
  {
    size_t
    operator()(const llarp::IPRange& range) const
    {
      const auto str = range.ToString();
      return std::hash<std::string>{}(str);
    }
  };
}  // namespace std
