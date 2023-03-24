#pragma once

#include <llarp/util/formattable.hpp>

namespace llarp::layers::platform
{
  /// the kind of traffic we are tunneling
  enum class EtherType_t
  {
    /// ipv4/ipv6 unicast traffic
    ip_unicast,
    /// plainquic stream
    plainquic,
    /// flow layer auth protocol
    proto_auth
  };

  std::string
  ToString(EtherType_t kind);
}  // namespace llarp::layers::platform

namespace llarp
{
  template <>
  inline constexpr bool IsToStringFormattable<layers::platform::EtherType_t> = true;
}
