#pragma once

#include <llarp/util/formattable.hpp>

#include <cstdint>
#include <ostream>

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

  };

  constexpr std::string_view
  ToString(ProtocolType t)
  {
    using namespace std::literals;
    return t == ProtocolType::Control  ? "Control"sv
        : t == ProtocolType::TrafficV4 ? "TrafficV4"sv
        : t == ProtocolType::TrafficV6 ? "TrafficV6"sv
        : t == ProtocolType::Exit      ? "Exit"sv
        : t == ProtocolType::Auth      ? "Auth"sv
        : t == ProtocolType::QUIC      ? "QUIC"sv
                                       : "(unknown-protocol-type)"sv;
  }

}  // namespace llarp::service

template <>
constexpr inline bool llarp::IsToStringFormattable<llarp::service::ProtocolType> = true;
