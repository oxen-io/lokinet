#include "address.hpp"
#include <cstring>
#include <iostream>

namespace llarp::quic
{
  using namespace std::literals;

  Address::Address(service::ConvoTag tag) : saddr{tag.ToV6()}
  {}

  Address&
  Address::operator=(const Address& other)
  {
    std::memmove(&saddr, &other.saddr, sizeof(saddr));
    a.addrlen = other.a.addrlen;
    return *this;
  }

  service::ConvoTag
  Address::Tag() const
  {
    service::ConvoTag tag{};
    tag.FromV6(saddr);
    return tag;
  }

  std::string
  Address::to_string() const
  {
    if (a.addrlen != sizeof(sockaddr_in6))
      return "(unknown-addr)";
    char buf[INET6_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET6, &saddr.sin6_addr, buf, INET6_ADDRSTRLEN);
    return buf;
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
