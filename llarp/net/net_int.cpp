#include "net_int.hpp"
#include "ip.hpp"
#include "llarp/net/ip_range.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>

#include <oxenc/endian.h>
#include <oxen/log/format.hpp>

using namespace oxen::log::literals;

namespace llarp
{
  namespace net
  {

    huint16_t
    ToHost(port_t x)
    {
      return huint16_t{oxenc::big_to_host(x.n)};
    }

    huint32_t
    ToHost(ipv4addr_t x)
    {
      return huint32_t{oxenc::big_to_host(x.n)};
    }

    huint128_t
    ToHost(ipv6addr_t x)
    {
      return {ntoh128(x.n)};
    }

    port_t
    ToNet(huint16_t x)
    {
      return port_t{oxenc::host_to_big(x.h)};
    }

    ipv4addr_t
    ToNet(huint32_t x)
    {
      return ipv4addr_t{oxenc::host_to_big(x.h)};
    }

    ipv6addr_t
    ToNet(huint128_t x)
    {
      return ipv6addr_t{hton128(x.h)};
    }

    ipv6addr_t
    maybe_expand_ip(ipaddr_t ip)
    {
      if (auto* ptr = std::get_if<ipv6addr_t>(&ip))
        return *ptr;
      return ToNet(ExpandV4(ToHost(std::get<ipv4addr_t>(ip))));
    }

    ipaddr_t
    maybe_truncate_ip(ipv6addr_t ip)
    {
      if (IPRange::V4MappedRange().Contains(ip))
        return ToNet(TruncateV6(ToHost(ip)));
      return ip;
    }

    ipaddr_t
    ipaddr_from_string(const std::string& str)
    {
      ipv6addr_t v6{};
      if (v6.FromString(str))
        return v6;
      ipv4addr_t v4{};
      if (v4.FromString(str))
        return v4;
      throw std::invalid_argument{"'{}' is not an valid ip address"_format(str)};
    }
  }  // namespace net

  template <>
  void
  huint32_t::ToV6(V6Container& c)
  {
    c.resize(16);
    std::fill(c.begin(), c.end(), 0);
    oxenc::write_host_as_big(h, c.data() + 12);
    c[11] = 0xff;
    c[10] = 0xff;
  }

  template <>
  void
  huint128_t::ToV6(V6Container& c)
  {
    c.resize(16);
    const in6_addr addr = net::HUIntToIn6(*this);
    std::copy_n(addr.s6_addr, 16, c.begin());
  }

  template <>
  std::string
  huint32_t::ToString() const
  {
    uint32_t n = htonl(h);
    char tmp[INET_ADDRSTRLEN] = {0};
    if (!inet_ntop(AF_INET, (void*)&n, tmp, sizeof(tmp)))
      return "";
    return tmp;
  }

  template <>
  std::string
  huint128_t::ToString() const
  {
    auto addr = ntoh128(h);
    char tmp[INET6_ADDRSTRLEN] = {0};
    if (!inet_ntop(AF_INET6, (void*)&addr, tmp, sizeof(tmp)))
      return "";
    return tmp;
  }

  template <>
  bool
  huint16_t::FromString(const std::string& str)
  {
    if (auto val = std::atoi(str.c_str()); val >= 0)
    {
      h = val;
      return true;
    }
    else
      return false;
  }

  template <>
  bool
  huint32_t::FromString(const std::string& str)
  {
    uint32_t n;
    if (!inet_pton(AF_INET, str.c_str(), &n))
      return false;
    h = ntohl(n);
    return true;
  }

  template <>
  bool
  huint128_t::FromString(const std::string& str)
  {
    llarp::uint128_t i;
    if (!inet_pton(AF_INET6, str.c_str(), &i))
      return false;
    h = ntoh128(i);
    return true;
  }

  template <>
  std::string
  nuint32_t::ToString() const
  {
    char tmp[INET_ADDRSTRLEN] = {0};
    if (!inet_ntop(AF_INET, (void*)&n, tmp, sizeof(tmp)))
      return "";
    return tmp;
  }

  template <>
  std::string
  nuint128_t::ToString() const
  {
    char tmp[INET6_ADDRSTRLEN] = {0};
    if (!inet_ntop(AF_INET6, (void*)&n, tmp, sizeof(tmp)))
      return "";
    return tmp;
  }

  template <>
  std::string
  huint16_t::ToString() const
  {
    return std::to_string(h);
  }

  template <>
  std::string
  nuint16_t::ToString() const
  {
    return std::to_string(ntohs(n));
  }

  std::string
  net::ToString(const ipaddr_t& ipaddr)
  {
    return std::visit([](const auto& ip) { return ip.ToString(); }, ipaddr);
  }

}  // namespace llarp
