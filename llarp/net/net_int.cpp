#include <net/net_int.hpp>
#include <net/ip.hpp>
#include <string>

namespace llarp
{
  template <>
  huint32_t
  ToHost(nuint32_t n)
  {
    return xntohl(n);
  }

  template <>
  nuint32_t
  ToNet(huint32_t h)
  {
    return xhtonl(h);
  }

  template <>
  void
  huint32_t::ToV6(V6Container& c)
  {
    c.resize(16);
    std::fill(c.begin(), c.end(), 0);
    htobe32buf(c.data() + 12, h);
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
}  // namespace llarp
