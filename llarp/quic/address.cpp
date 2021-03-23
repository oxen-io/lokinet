#include "address.hpp"
#include <cstring>
#include <iostream>

namespace llarp::quic
{
  using namespace std::literals;

  Address::Address(const SockAddr& addr) : saddr{*addr.operator const sockaddr_in6*()}
  {}

  Address&
  Address::operator=(const Address& other)
  {
    std::memmove(&saddr, &other.saddr, sizeof(saddr));
    a.addrlen = other.a.addrlen;
    return *this;
  }

  Address::operator service::ConvoTag() const
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
    std::string result;
    result.resize(8 + INET6_ADDRSTRLEN);
    result[0] = '[';
    inet_ntop(AF_INET6, &saddr.sin6_addr, &result[1], INET6_ADDRSTRLEN);
    result.resize(result.find(char{0}));
    result += "]:";
    result += std::to_string(ToHost(nuint16_t{saddr.sin6_port}).h);
    return result;
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
