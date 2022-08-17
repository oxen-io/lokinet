#pragma once
#include <chrono>

namespace llarp::constants
{
  /// current network wide protocol version
  // TODO: enum class
  constexpr auto proto_version = 0;

  /// current router contact format version
  constexpr auto rc_version = 1;

  /// average time between blocks from backend
  constexpr auto block_interval = std::chrono::minutes{2};

}  // namespace llarp::constants
