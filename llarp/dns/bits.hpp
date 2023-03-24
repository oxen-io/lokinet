#pragma once

#include <cstdint>

namespace llarp::dns::bits
{

  /// query reply bitmask
  constexpr uint16_t query_reply = 1 << 15;
  /// authoritative response bitmask
  constexpr uint16_t authoritative = 1 << 10;
  /// recursion desirsed bitmask
  constexpr uint16_t recursion_desired = 1 << 8;
  // recursion allowed bitmask
  constexpr uint16_t recursion_allowed = 1 << 7;
  /// rcode for nx reply
  constexpr uint16_t rcode_name_error = 3;
  /// rcdode for srv fail
  constexpr uint16_t rcode_servfail = 2;
  /// recode for no error
  constexpr uint16_t rcode_no_error = 0;

  constexpr uint16_t qclass_in = 1;

  inline constexpr uint16_t
  make_rcode(uint16_t rcode)
  {
    return (rcode | query_reply | authoritative | recursion_allowed) & (~recursion_desired);
  }
}  // namespace llarp::dns::bits
