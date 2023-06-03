#include "sock_addr.hpp"
#include "ip_range.hpp"
#include "address_info.hpp"
#include "ip.hpp"
#include "net_bits.hpp"
#include "net.hpp"
#include <llarp/util/str.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/util/mem.hpp>

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
    m_addr.sin6_family = AF_INET6;
    llarp::Zero(&m_addr4, sizeof(m_addr4));
    m_addr4.sin_family = AF_INET;
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

  SockAddr::SockAddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d, huint16_t port)
  {
    init();
    setIPv4(a, b, c, d);
    setPort(port);
  }

  SockAddr::SockAddr(nuint32_t ip, nuint16_t port)
  {
    init();
    setIPv4(ip);
    setPort(port);
  }

  SockAddr::SockAddr(huint32_t ip, huint16_t port) : SockAddr{ToNet(ip), ToNet(port)}
  {}

  SockAddr::SockAddr(huint128_t ip, huint16_t port)
  {
    init();
    setIPv6(ip);
    setPort(port);
  }

  SockAddr::SockAddr(nuint128_t ip, nuint16_t port)
  {
    init();
    setIPv6(ip);
    setPort(port);
  }

  SockAddr::SockAddr(std::string_view addr)
  {
    init();
    fromString(addr);
  }

  SockAddr::SockAddr(std::string_view addr, huint16_t port)
  {
    init();
    setPort(port);
    fromString(addr, false);
  }

  SockAddr::SockAddr(const AddressInfo& info) : SockAddr{info.ip}
  {
    setPort(huint16_t{info.port});
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
      throw std::invalid_argument{
          fmt::format("Invalid sockaddr (not AF_INET or AF_INET6) was {}", other.sa_family)};

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
    if (IPRange::V4MappedRange().Contains(asIPv6()))
    {
      setIPv4(
          other.sin6_addr.s6_addr[12],
          other.sin6_addr.s6_addr[13],
          other.sin6_addr.s6_addr[14],
          other.sin6_addr.s6_addr[15]);
      m_addr4.sin_port = m_addr.sin6_port;
    }
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
    if (IPRange::V4MappedRange().Contains(asIPv6()))
    {
      setIPv4(other.s6_addr[12], other.s6_addr[13], other.s6_addr[14], other.s6_addr[15]);
      m_addr4.sin_port = m_addr.sin6_port;
    }
    m_empty = false;

    return *this;
  }

  SockAddr::operator const sockaddr*() const
  {
    return isIPv4() ? reinterpret_cast<const sockaddr*>(&m_addr4)
                    : reinterpret_cast<const sockaddr*>(&m_addr);
  }

  SockAddr::operator const sockaddr_in*() const
  {
    return &m_addr4;
  }

  SockAddr::operator const sockaddr_in6*() const
  {
    return &m_addr;
  }

  size_t
  SockAddr::sockaddr_len() const
  {
    return isIPv6() ? sizeof(m_addr) : sizeof(m_addr4);
  }

  bool
  SockAddr::operator<(const SockAddr& other) const
  {
    return m_addr < other.m_addr;
  }

  bool
  SockAddr::operator==(const SockAddr& other) const
  {
    return m_addr == other.m_addr;
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
  SockAddr::fromString(std::string_view str, bool allow_port)
  {
    if (str.empty())
    {
      init();
      m_empty = true;
      return;
    }

    // NOTE: this potentially involves multiple memory allocations,
    //       reimplement without split() if it is performance bottleneck
    auto splits = split(str, ":");

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
    auto ipSplits = split(splits[0], ".");
    if (ipSplits.size() != 4)
      throw std::invalid_argument(fmt::format("{} is not a valid IPv4 address", str));

    std::array<uint8_t, 4> ipBytes;
    for (int i = 0; i < 4; ++i)
      if (not parse_int(ipSplits[i], ipBytes[i]))
        throw std::runtime_error(fmt::format("{} contains invalid numeric value", str));

    // attempt port before setting IPv4 bytes
    if (splits.size() == 2)
    {
      if (not allow_port)
        throw std::runtime_error{
            fmt::format("invalid ip address (port not allowed here): {}", str)};
      uint16_t port;
      if (not parse_int(splits[1], port))
        throw std::runtime_error{fmt::format("{} is not a valid port", splits[1])};
      setPort(port);
    }

    setIPv4(ipBytes[0], ipBytes[1], ipBytes[2], ipBytes[3]);
  }

  std::string
  SockAddr::ToString() const
  {
    // TODO: review
    if (isEmpty())
      return "";
    return fmt::format("{}:{}", hostString(), port());
  }

  std::string
  SockAddr::hostString(bool ipv6_brackets) const
  {
    std::array<char, 128> buf{};
    if (isIPv4())
    {
      // IPv4 mapped addrs
      inet_ntop(AF_INET, &m_addr4.sin_addr.s_addr, buf.data(), buf.size());
      return buf.data();
    }

    inet_ntop(AF_INET6, &m_addr.sin6_addr.s6_addr, buf.data(), buf.size());
    if (not ipv6_brackets)
      return buf.data();

    return fmt::format("[{}]", buf.data());
  }

  bool
  SockAddr::isEmpty() const
  {
    return m_empty;
  }

  bool
  SockAddr::isIPv4() const
  {
    return IPRange::V4MappedRange().Contains(asIPv6());
  }
  bool
  SockAddr::isIPv6() const
  {
    return not isIPv4();
  }

  nuint32_t
  SockAddr::getIPv4() const
  {
    return {m_addr4.sin_addr.s_addr};
  }

  nuint128_t
  SockAddr::getIPv6() const
  {
    nuint128_t a;
    // Explicit cast to void* here to avoid non-trivial type copying warnings (technically this
    // isn't trivial because of the zeroing default constructor, but it's trivial enough that this
    // copy is safe).
    std::memcpy(static_cast<void*>(&a), &m_addr.sin6_addr, 16);
    return a;
  }

  std::variant<nuint32_t, nuint128_t>
  SockAddr::getIP() const
  {
    if (isIPv4())
      return getIPv4();
    return getIPv6();
  }

  void
  SockAddr::setIPv4(nuint32_t ip)
  {
    uint8_t* ip6 = m_addr.sin6_addr.s6_addr;
    llarp::Zero(ip6, sizeof(m_addr.sin6_addr.s6_addr));

    applyIPv4MapBytes();

    std::memcpy(ip6 + 12, &ip, 4);
    m_addr4.sin_addr.s_addr = ip.n;
    m_empty = false;
  }

  void
  SockAddr::setIPv4(huint32_t ip)
  {
    setIPv4(ToNet(ip));
  }

  void
  SockAddr::setIPv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
  {
    uint8_t* ip6 = m_addr.sin6_addr.s6_addr;
    llarp::Zero(ip6, sizeof(m_addr.sin6_addr.s6_addr));

    applyIPv4MapBytes();

    ip6[12] = a;
    ip6[13] = b;
    ip6[14] = c;
    ip6[15] = d;
    const auto ip = ipaddr_ipv4_bits(a, b, c, d);
    m_addr4.sin_addr.s_addr = htonl(ip.h);
    m_empty = false;
  }

  void
  SockAddr::setIPv6(huint128_t ip)
  {
    return setIPv6(ToNet(ip));
  }

  void
  SockAddr::setIPv6(nuint128_t ip)
  {
    std::memcpy(&m_addr.sin6_addr, &ip, sizeof(m_addr.sin6_addr));
    if (isIPv4())
    {
      setIPv4(
          m_addr.sin6_addr.s6_addr[12],
          m_addr.sin6_addr.s6_addr[13],
          m_addr.sin6_addr.s6_addr[14],
          m_addr.sin6_addr.s6_addr[15]);
      m_addr4.sin_port = m_addr.sin6_port;
    }
  }

  void
  SockAddr::setPort(nuint16_t port)
  {
    m_addr.sin6_port = port.n;
    m_addr4.sin_port = port.n;
  }

  void
  SockAddr::setPort(huint16_t port)
  {
    setPort(ToNet(port));
  }

  net::port_t
  SockAddr::port() const
  {
    return net::port_t{m_addr.sin6_port};
  }

}  // namespace llarp
