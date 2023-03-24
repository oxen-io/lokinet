#pragma once
#include <llarp/util/formattable.hpp>

namespace llarp::layers::flow
{
  enum class FlowDataKind
  {
    unknown,
    /// unicast ip traffic that does NOT exit lokinet.
    direct_ip_unicast,
    /// unicast ip traffic that does exit lokinet.
    exit_ip_unicast,
    /// auth data.
    auth,
    /// unicast stream data.
    stream_unicast,
  };

  std::string_view
  ToString(FlowDataKind kind);
}  // namespace llarp::layers::flow

namespace llarp
{

  template <>
  inline constexpr bool IsToStringFormattable<llarp::layers::flow::FlowDataKind> = true;
}
