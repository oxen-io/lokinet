#pragma once

#include <cstdint>

#include <ostream>
#include <optional>

namespace llarp::service
{
  // Supported protocol types; the values are given explicitly because they are specifically used
  // when sending over the wire.
  enum class ProtocolType : uint64_t
  {
    Control = 0UL,
    TrafficV4 = 1UL,
    TrafficV6 = 2UL,
    Exit = 3UL,
    Auth = 4UL,
    QUIC = 5UL,
    PacketsV4 = 6UL,
    PacketsV6 = 7UL,
    PacketsExit = 8UL,
  };

  /// returns the anolog protocol type for a batched traffic if it exists
  std::optional<ProtocolType>
  BatchedProtoAsUnderlying(ProtocolType t);

  inline bool
  IsBatchedProto(ProtocolType t)
  {
    return BatchedProtoAsUnderlying(t) != std::nullopt;
  }

  std::optional<ProtocolType>
  ToBatchedProto(ProtocolType t);

  std::ostream&
  operator<<(std::ostream& o, ProtocolType t);

}  // namespace llarp::service
