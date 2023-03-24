#pragma once

#include <llarp/net/net_int.hpp>

#include <array>
#include <numeric>
#include <string>
#include <optional>
#include <functional>

namespace llarp::layers::platform
{

  /// a (ipv6 address, flowlabel) tuple that we use for platform layer side addressing
  struct PlatformAddr
  {
    /// ip network address
    net::ipv6addr_t ip{};

    /// flowlabel, ingored if ip is a v4 mapped address
    /// this is used as a metric to do flow layer isolation where we can use different identities
    /// and / or paths on each. defaults to zero.
    net::flowlabel_t flowlabel{};

    PlatformAddr() = default;
    PlatformAddr(const PlatformAddr&) = default;
    PlatformAddr(PlatformAddr&&) = default;

    PlatformAddr&
    operator=(const PlatformAddr&) = default;
    PlatformAddr&
    operator=(PlatformAddr&&) = default;

    explicit PlatformAddr(net::ipaddr_t addr);
    explicit PlatformAddr(const std::string& str);
    explicit PlatformAddr(huint128_t addr);

    /// string representation
    std::string
    ToString() const;

    /// convert to variant. compacts to ipv4 if it is a mapped address.
    net::ipaddr_t
    as_ipaddr() const;

    /// returns ipv4 address if it's a mapped address. otherwise returns nullopt.
    std::optional<net::ipv4addr_t>
    as_ipv4addr() const;

    bool
    operator==(const PlatformAddr& other) const;
    bool
    operator!=(const PlatformAddr& other) const;

    bool
    operator<(const PlatformAddr& other) const;
  };

}  // namespace llarp::layers::platform

namespace llarp
{
  template <>
  inline constexpr bool IsToStringFormattable<layers::platform::PlatformAddr> = true;
}

namespace std
{
  template <>
  struct hash<llarp::layers::platform::PlatformAddr>
  {
    size_t
    operator()(const llarp::layers::platform::PlatformAddr& addr) const
    {
      const std::array<uint64_t, 4> data{addr.ip.n.upper, addr.ip.n.lower, addr.flowlabel.n};
      int n{};
      return std::accumulate(
          data.begin(), data.end(), uint64_t{}, [&n](uint64_t lhs, uint64_t rhs) {
            return lhs ^ (rhs << (n += (n ? 2 : 1)));
          });
    }
  };
}  // namespace std
