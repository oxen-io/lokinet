#include <net/ip_address.hpp>

#include <net/net.hpp>

namespace llarp
{
  IpAddress::IpAddress(std::string_view str)
  {
    setAddress(str);
  }

  IpAddress::IpAddress(std::string_view str, std::optional<uint16_t> port)
  {
    setAddress(str, port);
  }

  IpAddress::IpAddress(const SockAddr& addr)
  {
    m_ipAddress = addr.toString();
    uint16_t port = addr.getPort();
    if (port > 0)
      m_port = port;

    m_empty = addr.isEmpty();
  }

  IpAddress&
  IpAddress::operator=(const sockaddr& other)
  {
    SockAddr addr(other);

    m_ipAddress = addr.toString();
    uint16_t port = addr.getPort();
    if (port > 0)
      m_port = port;

    m_empty = addr.isEmpty();

    return *this;
  }

  std::optional<uint16_t>
  IpAddress::getPort() const
  {
    return m_port;
  }

  void
  IpAddress::setPort(std::optional<uint16_t> port)
  {
    m_port = port;
  }

  void
  IpAddress::setAddress(std::string_view str)
  {
    SockAddr addr;
    addr.fromString(str);

    m_ipAddress = std::string(str);
    uint16_t port = addr.getPort();
    if (port > 0)
      m_port = port;

    m_empty = addr.isEmpty();
  }

  void
  IpAddress::setAddress(std::string_view str, std::optional<uint16_t> port)
  {
    SockAddr addr;
    addr.fromString(str);

    m_ipAddress = std::string(str);
    m_port = port;

    m_empty = addr.isEmpty();
  }

  bool
  IpAddress::isIPv4()
  {
    throw std::runtime_error("FIXME - IpAddress::isIPv4()");
  }

  bool
  IpAddress::isEmpty() const
  {
    return m_empty;
  }

  SockAddr
  IpAddress::createSockAddr() const
  {
    SockAddr addr(m_ipAddress);
    if (m_port)
      addr.setPort(*m_port);

    return addr;
  }

  bool
  IpAddress::isBogon() const
  {
    SockAddr addr(m_ipAddress);
    const sockaddr_in6* addr6 = addr;
    const uint8_t* raw = addr6->sin6_addr.s6_addr;
    return IsIPv4Bogon(ipaddr_ipv4_bits(raw[12], raw[13], raw[14], raw[15]));
  }

  std::string
  IpAddress::toString() const
  {
    return m_ipAddress;  // TODO: port
  }

  bool
  IpAddress::operator<(const IpAddress& other) const
  {
    return createSockAddr() < other.createSockAddr();
  }

  bool
  IpAddress::operator==(const IpAddress& other) const
  {
    return createSockAddr() == other.createSockAddr();
  }

  std::ostream&
  operator<<(std::ostream& out, const IpAddress& address)
  {
    out << address.toString();
    return out;
  }

}  // namespace llarp
