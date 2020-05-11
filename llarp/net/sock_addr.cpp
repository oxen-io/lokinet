#include <net/sock_addr.hpp>

#include <netinet/in.h>
#include <util/str.hpp>
#include <util/logging/logger.hpp>
#include <util/mem.hpp>

#include <charconv>
#include <stdexcept>

namespace llarp
{
  /// shared utility functions
  ///

  void
  SockAddr::init()
  {
    llarp::Zero(&m_addr, sizeof(m_addr));
  }

  void
  SockAddr::applySIITBytes()
  {
    uint8_t* ip6 = m_addr.sin6_addr.s6_addr;

    // SIIT == Stateless IP/ICMP Translation (represent IPv4 with IPv6)
    ip6[10] = 0xff;
    ip6[11] = 0xff;
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
  {
    init();
    setIPv4(a, b, c, d);
    setPort(port);
  }

  SockAddr::SockAddr(std::string_view addr)
  {
    init();
    fromString(addr);
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
      *this = (const sockaddr_in6&)other;
    else if (other.sa_family == AF_INET)
      *this = (const sockaddr_in&)other;
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
    applySIITBytes();

    // avoid byte order conversion (this is NBO -> NBO)
    memcpy(m_addr.sin6_addr.s6_addr + 12, &other.sin_addr.s_addr, sizeof(in_addr));
    m_addr.sin6_port = other.sin_port;

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

    m_empty = false;

    return *this;
  }

  SockAddr::operator const sockaddr*() const
  {
    return (sockaddr*)&m_addr;
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
    if (m_addr.sin6_family != other.m_addr.sin6_family)
      return false;

    if (getPort() != other.getPort())
      return false;

    return (
        0
        == memcmp(
            m_addr.sin6_addr.s6_addr,
            other.m_addr.sin6_addr.s6_addr,
            sizeof(m_addr.sin6_addr.s6_addr)));
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
      throw std::runtime_error("IPv6 not yet supported");

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

    uint8_t* ip6 = m_addr.sin6_addr.s6_addr;

    // ensure SIIT
    if (ip6[10] != 0xff or ip6[11] != 0xff)
      throw std::runtime_error("Only SIIT address supported");

    constexpr auto MaxIPv4PlusPortStringSize = 22;
    std::string str;
    str.reserve(MaxIPv4PlusPortStringSize);

    // TODO: ensure these don't each incur a memory allocation
    str.append(std::to_string(ip6[12]));
    str.append(1, '.');
    str.append(std::to_string(ip6[13]));
    str.append(1, '.');
    str.append(std::to_string(ip6[14]));
    str.append(1, '.');
    str.append(std::to_string(ip6[15]));

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
    m_addr.sin6_family = AF_INET6;

    uint8_t* ip6 = m_addr.sin6_addr.s6_addr;
    llarp::Zero(ip6, sizeof(m_addr.sin6_addr.s6_addr));

    applySIITBytes();

    ip6[12] = a;
    ip6[13] = b;
    ip6[14] = c;
    ip6[15] = d;

    m_empty = false;
  }

  void
  SockAddr::setPort(uint16_t port)
  {
    m_addr.sin6_port = htons(port);
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
