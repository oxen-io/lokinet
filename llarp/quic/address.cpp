#include "address.hpp"

extern "C"
{
#include <arpa/inet.h>
}

#include <iostream>

namespace llarp::quic
{
  using namespace std::literals;

  Address::Address(std::array<uint8_t, 4> ip, uint16_t port)
  {
    s.in.sin_family = AF_INET;
    std::memcpy(&s.in.sin_addr.s_addr, ip.data(), ip.size());
    s.in.sin_port = htons(port);
    a.addrlen = sizeof(s.in);
  }

  Address::Address(const sockaddr_any* addr, size_t addrlen)
  {
    assert(addrlen == sizeof(sockaddr_in));  // FIXME: IPv6 support
    std::memmove(&s, addr, addrlen);
    a.addrlen = addrlen;
  }
  Address&
  Address::operator=(const Address& addr)
  {
    std::memmove(&s, &addr.s, sizeof(s));
    a.addrlen = addr.a.addrlen;
    return *this;
  }

  std::string
  Address::to_string() const
  {
    if (a.addrlen != sizeof(sockaddr_in))
      return "(unknown-addr)";
    char buf[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &s.in.sin_addr, buf, INET_ADDRSTRLEN);
    return buf + ":"s + std::to_string(ntohs(s.in.sin_port));
  }

  std::ostream&
  operator<<(std::ostream& o, const Address& a)
  {
    return o << a.to_string();
  }
  std::ostream&
  operator<<(std::ostream& o, const Path& p)
  {
    return o << p.local << "<-" << p.remote;
  }

}  // namespace llarp::quic
