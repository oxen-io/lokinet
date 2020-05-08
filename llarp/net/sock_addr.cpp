#include <net/sock_addr.hpp>

#include <netinet/in.h>
#include <util/str.hpp>
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
    throw std::runtime_error("FIXME");
  }

  SockAddr::SockAddr(const SockAddr&)
  {
    throw std::runtime_error("FIXME");
  }

  SockAddr&
  SockAddr::operator=(const SockAddr&) const
  {
    throw std::runtime_error("FIXME");
  }

  SockAddr::SockAddr(const sockaddr& addr)
  {
    throw std::runtime_error("FIXME");
  }

  SockAddr::SockAddr(const sockaddr_in& addr)
  {
    throw std::runtime_error("FIXME");
  }

  SockAddr::operator const sockaddr*() const
  {
    throw std::runtime_error("FIXME");
  }

  void
  SockAddr::fromString(std::string_view str)
  {
    if (str.empty())
      throw std::invalid_argument("cannot construct IPv4 from empty string");

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

      if (result.ec == std::errc::invalid_argument)
        throw std::runtime_error(stringify(str, " contains invalid number"));
    }

    // attempt port before setting IPv4 bytes
    if (splits.size() == 2)
    {
      uint16_t port = 0;
      auto portStr = splits[1];
      auto result = std::from_chars(portStr.data(), portStr.data() + portStr.size(), port);

      if (result.ec == std::errc::invalid_argument)
        throw std::runtime_error(stringify(str, " contains invalid port"));

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

    if (m_addr.sin6_family != AF_INET)
      throw std::runtime_error("Only IPv4 supported");

    uint8_t* ip6 = m_addr.sin6_addr.s6_addr;

    // ensure SIIT
    if (not ip6[10] == 0xff or not ip6[11])
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
    str.append(std::to_string(m_addr.sin6_port));

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

    // SIIT (represent IPv4 with IPv6)
    ip6[10] = 0xff;
    ip6[11] = 0xff;

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
    throw std::runtime_error("FIXME");
  }

}  // namespace llarp
