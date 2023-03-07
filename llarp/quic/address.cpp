#include "address.hpp"
#include <cstring>
#include <iostream>
#include <optional>

namespace llarp::quic
{
  using namespace std::literals;

  Address::Address(const SockAddr& addr, std::optional<std::variant<service::Address, RouterID>> ep)
      : saddr{*addr.operator const sockaddr_in6*()}, endpoint{ep}
  {}

  Address&
  Address::operator=(const Address& other)
  {
    std::memmove(&saddr, &other.saddr, sizeof(saddr));
    a.addrlen = other.a.addrlen;
    endpoint = other.endpoint;
    return *this;
  }

  Address::operator service::ConvoTag() const
  {
    service::ConvoTag tag{};
    tag.FromV6(saddr);
    return tag;
  }

  std::string
  Address::ToString() const
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

  // FIXME: remote now has std::variant
  std::string
  Path::ToString() const
  {
    return local.ToString() + "<-" + remote.ToString();
  }

}  // namespace llarp::quic
