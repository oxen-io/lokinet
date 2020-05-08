#include <net/ip_address.hpp>

namespace llarp
{
  IpAddress::IpAddress(std::string_view str)
  {
    throw std::runtime_error("FIXME");
  }

  IpAddress::IpAddress(std::string_view str, std::optional<uint16_t> port)
  {
    throw std::runtime_error("FIXME");
  }

  IpAddress::IpAddress(const SockAddr& addr)
  {
    throw std::runtime_error("FIXME");
  }

  SockAddr&
  IpAddress::operator=(const sockaddr& other)
  {
    throw std::runtime_error("FIXME");
  }

  std::optional<uint16_t>
  IpAddress::getPort() const
  {
    throw std::runtime_error("FIXME");
  }

  void
  IpAddress::setPort(std::optional<uint16_t> port)
  {
    throw std::runtime_error("FIXME");
  }

  void
  IpAddress::setAddress(std::string_view str)
  {
    throw std::runtime_error("FIXME");
  }

  void
  IpAddress::setAddress(std::string_view str, std::optional<uint16_t> port)
  {
    throw std::runtime_error("FIXME");
  }

  bool
  IpAddress::isIPv4()
  {
    throw std::runtime_error("FIXME");
  }

  bool
  IpAddress::isEmpty() const
  {
    throw std::runtime_error("FIXME");
  }

  SockAddr
  IpAddress::createSockAddr() const
  {
    throw std::runtime_error("FIXME");
  }

  bool
  IpAddress::isBogon() const
  {
    throw std::runtime_error("FIXME");
  }

  std::string
  IpAddress::toString() const
  {
    throw std::runtime_error("FIXME");
  }

  bool
  IpAddress::operator<(const IpAddress& other) const
  {
    throw std::runtime_error("FIXME");
  }

  bool
  IpAddress::operator==(const IpAddress& other) const
  {
    throw std::runtime_error("FIXME");
  }

  std::ostream&
  operator<<(std::ostream& out, const IpAddress& address)
  {
    throw std::runtime_error("FIXME");
  }

}  // namespace llarp
