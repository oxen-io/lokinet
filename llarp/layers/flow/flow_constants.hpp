#pragma once
#include <cstdint>

namespace llarp::layers::flow
{
  /// the mtu of plaintext data we transport via the flow layer.
  static inline constexpr uint16_t default_flow_mtu = 1500;
  /// maximum mtu we can carry of the flow layer
  // TODO: verify
  static inline constexpr uint16_t max_flow_mtu = 4096;

}  // namespace llarp::layers::flow
