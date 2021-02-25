#include <net/sock_addr.hpp>
#include <net/address_info.hpp>
#include <net/ip.hpp>
#include <net/net_bits.hpp>
#include <util/str.hpp>
#include <util/logging/logger.hpp>
#include <util/mem.hpp>

#include <charconv>
#include <stdexcept>

namespace llarp
{
  bool
  operator==(const in6_addr& lh, const in6_addr& rh)
  {
    return memcmp(&lh, &rh, sizeof(in6_addr)) == 0;
  }
  /// shared utility functions
  ///

  void
  SockAddr::init()
  {
    llarp::Zero(&m_addr, sizeof(m_addr));
    llarp::Zero(&m_addr4, sizeof(m_addr4));
  }

  void
  SockAddr::applyIPv4MapBytes()
  {
    std::memcpy(m_addr.sin6_addr.s6_addr, ipv4_map_prefix.data(), ipv4_map_prefix.size());
  }

  SockAddr::SockAddr()
  {
    init();
  }

  SockAddr::SockAddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
  {
    init();
    setIPv4(a, b, c, d);
  }

  SockAddr::SockAddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port)
      : SockAddr{a, b, c, d}
  {
    setPort(port);
  }
  SockAddr::SockAddr(std::string_view addr)
  {
    init();
    fromString(addr);
  }

  SockAddr::SockAddr(const AddressInfo& info) : SockAddr{info.ip}
  {
    setPort(info.port);
  }

  SockAddr::SockAddr(const SockAddr& other)
  {
    *this = other;
  }

  SockAddr&
  SockAddr::operator=(const SockAddr& other)
  {
    *this = other.m_addr;
    return *this;
  }

  SockAddr::SockAddr(const sockaddr& addr)
  {
    *this = addr;
  }

  SockAddr&
  SockAddr::operator=(const sockaddr& other)
  {
    if (other.sa_family == AF_INET6)
      *this = reinterpret_cast<const sockaddr_in6&>(other);
    else if (other.sa_family == AF_INET)
      *this = reinterpret_cast<const sockaddr_in&>(other);
    else
      throw std::invalid_argument("Invalid sockaddr (not AF_INET or AF_INET6)");

    return *this;
  }

  SockAddr::SockAddr(const sockaddr_in& addr)
  {
    *this = addr;
  }

  SockAddr&
  SockAddr::operator=(const sockaddr_in& other)
  {
    init();
    applyIPv4MapBytes();

    // avoid byte order conversion (this is NBO -> NBO)
    memcpy(m_addr.sin6_addr.s6_addr + 12, &other.sin_addr.s_addr, sizeof(in_addr));
    m_addr.sin6_port = other.sin_port;
    m_addr4.sin_addr.s_addr = other.sin_addr.s_addr;
    m_addr4.sin_port = other.sin_port;
    m_empty = false;

    return *this;
  }

  SockAddr::SockAddr(const sockaddr_in6& addr)
  {
    *this = addr;
  }

  SockAddr&
  SockAddr::operator=(const sockaddr_in6& other)
  {
    init();

    memcpy(&m_addr, &other, sizeof(sockaddr_in6));
    if (ipv6_is_mapped_ipv4(other.sin6_addr))
      setIPv4(
          other.sin6_addr.s6_addr[12],
          other.sin6_addr.s6_addr[13],
          other.sin6_addr.s6_addr[14],
          other.sin6_addr.s6_addr[15]);
    setPort(ntohs(other.sin6_port));
    m_empty = false;

    return *this;
  }

  SockAddr::SockAddr(const in6_addr& addr)
  {
    *this = addr;
  }

  SockAddr&
  SockAddr::operator=(const in6_addr& other)
  {
    init();

    memcpy(&m_addr.sin6_addr.s6_addr, &other.s6_addr, sizeof(m_addr.sin6_addr.s6_addr));
    if (ipv6_is_mapped_ipv4(other))
      setIPv4(other.s6_addr[12], other.s6_addr[13], other.s6_addr[14], other.s6_addr[15]);
    m_empty = false;

    return *this;
  }

  SockAddr::operator const sockaddr*() const
  {
    return (sockaddr*)&m_addr;
  }

  SockAddr::operator const sockaddr_in*() const
  {
    return &m_addr4;
  }

  SockAddr::operator const sockaddr_in6*() const
  {
    return &m_addr;
  }

  bool
  SockAddr::operator<(const SockAddr& other) const
  {
    return (m_addr.sin6_addr.s6_addr < other.m_addr.sin6_addr.s6_addr);
  }

  bool
  SockAddr::operator==(const SockAddr& other) const
  {
    return m_addr.sin6_addr == other.m_addr.sin6_addr
        and m_addr.sin6_port == other.m_addr.sin6_port;
  }

  huint128_t
  SockAddr::asIPv6() const
  {
    return net::In6ToHUInt(m_addr.sin6_addr);
  }

  huint32_t
  SockAddr::asIPv4() const
  {
    const nuint32_t n{m_addr4.sin_addr.s_addr};
    return ToHost(n);
  }

  void
  SockAddr::fromString(std::string_view str)
  {
    if (str.empty())
    {
      init();
      m_empty = true;
      return;
    }

    // NOTE: this potentially involves multiple memory allocations,
    //       reimplement without split() if it is performance bottleneck
    auto splits = split(str, ':');

    // TODO: having ":port" at the end makes this ambiguous with IPv6
    //       come up with a strategy for implementing
    if (splits.size() > 2)
    {
      std::string data{str};
      if (inet_pton(AF_INET6, data.c_str(), m_addr.sin6_addr.s6_addr) == -1)
        throw std::runtime_error{"invalid ip6 address: " + data};
      return;
    }

    // split() shouldn't return an empty list if str is empty (checked above)
    assert(splits.size() > 0);

    // splits[0] should be dot-separated IPv4
    auto ipSplits = split(splits[0], '.');
    if (ipSplits.size() != 4)
      throw std::invalid_argument(stringify(str, " is not a valid IPv4 address"));

    uint8_t ipBytes[4] = {0};

    for (int i = 0; i < 4; ++i)
    {
      auto byteStr = ipSplits[i];
      auto result = std::from_chars(byteStr.data(), byteStr.data() + byteStr.size(), ipBytes[i]);

      if (result.ec != std::errc())
        throw std::runtime_error(stringify(str, " contains invalid number"));

      if (result.ptr != (byteStr.data() + byteStr.size()))
        throw std::runtime_error(stringify(str, " contains non-numeric values"));
    }

    // attempt port before setting IPv4 bytes
    if (splits.size() == 2)
    {
      uint16_t port = 0;
      auto portStr = splits[1];
      auto result = std::from_chars(portStr.data(), portStr.data() + portStr.size(), port);

      if (result.ec != std::errc())
        throw std::runtime_error(stringify(str, " contains invalid port"));

      if (result.ptr != (portStr.data() + portStr.size()))
        throw std::runtime_error(stringify(str, " contains junk after port"));

      setPort(port);
    }

    setIPv4(ipBytes[0], ipBytes[1], ipBytes[2], ipBytes[3]);
  }

  std::string
  SockAddr::toString() const
  {
    // TODO: review
    if (isEmpty())
      return "";

    const uint8_t* ip6 = m_addr.sin6_addr.s6_addr;
    std::string str;

    if (ip6[10] == 0xff and ip6[11] == 0xff)
    {
      // handle IPv4 mapped addrs
      constexpr auto MaxIPv4PlusPortStringSize = 22;
      str.reserve(MaxIPv4PlusPortStringSize);
      char buf[128] = {0x0};
      inet_ntop(AF_INET, &m_addr4.sin_addr.s_addr, buf, sizeof(buf));
      str.append(buf);
    }
    else
    {
      constexpr auto MaxIPv6PlusPortStringSize = 128;
      str.reserve(MaxIPv6PlusPortStringSize);

      char buf[128] = {0x0};
      inet_ntop(AF_INET6, &m_addr.sin6_addr.s6_addr, buf, sizeof(buf));

      str.append("[");
      str.append(buf);
      str.append("]");
    }

    str.append(1, ':');
    str.append(std::to_string(getPort()));
    return str;
  }

  bool
  SockAddr::isEmpty() const
  {
    return m_empty;
  }

  void
  SockAddr::setIPv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
  {
    m_addr.sin6_family = AF_INET;

    uint8_t* ip6 = m_addr.sin6_addr.s6_addr;
    llarp::Zero(ip6, sizeof(m_addr.sin6_addr.s6_addr));

    applyIPv4MapBytes();

    ip6[12] = a;
    ip6[13] = b;
    ip6[14] = c;
    ip6[15] = d;
    const auto ip = ipaddr_ipv4_bits(a, b, c, d);
    m_addr4.sin_addr.s_addr = htonl(ip.h);
    m_addr4.sin_family = AF_INET;
    m_empty = false;
  }

  void
  SockAddr::setPort(uint16_t port)
  {
    m_addr.sin6_port = htons(port);
    m_addr4.sin_port = htons(port);
  }

  uint16_t
  SockAddr::getPort() const
  {
    return ntohs(m_addr.sin6_port);
  }

  std::ostream&
  operator<<(std::ostream& out, const SockAddr& address)
  {
    out << address.toString();
    return out;
  }

}  // namespace llarp
