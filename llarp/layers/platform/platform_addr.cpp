#include "platform_addr.hpp"
#include <optional>
#include <type_traits>
#include "llarp/layers/platform/platform_layer.hpp"
#include "llarp/net/ip.hpp"
#include "llarp/net/ip_range.hpp"
#include "llarp/net/net_int.hpp"

namespace llarp::layers::platform
{

  PlatformAddr::PlatformAddr(huint128_t addr) : PlatformAddr{ToNet(addr)}
  {}

  PlatformAddr::PlatformAddr(net::ipaddr_t addr) : ip{net::maybe_expand_ip(addr)}
  {}

  PlatformAddr::PlatformAddr(const std::string& str) : PlatformAddr{net::ipaddr_from_string(str)}
  {}

  net::ipaddr_t
  PlatformAddr::as_ipaddr() const
  {
    return net::maybe_truncate_ip(ip);
  }

  std::optional<net::ipv4addr_t>
  PlatformAddr::as_ipv4addr() const
  {
    auto ip = as_ipaddr();
    if (auto* ptr = std::get_if<net::ipv4addr_t>(&ip))
      return *ptr;
    return std::nullopt;
  }

  bool
  PlatformAddr::operator==(const PlatformAddr& other) const
  {
    return ip == other.ip;
  }
  bool
  PlatformAddr::operator!=(const PlatformAddr& other) const
  {
    return not(*this == other);
  }

  bool
  PlatformAddr::operator<(const PlatformAddr& other) const
  {
    return ToHost(ip) < ToHost(other.ip);
  }

  std::string
  PlatformAddr::ToString() const
  {
    return fmt::format(
        "[PlatformAddr '{}' flowlabel={:x}']",
        var::visit([](auto&& addr) { return addr.ToString(); }, as_ipaddr()),
        ToHost(flowlabel).h);
  }
}  // namespace llarp::layers::platform
