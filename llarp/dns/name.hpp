#pragma once

#include <llarp/net/net_int.hpp>
#include <llarp/util/buffer.hpp>
#include <string>
#include <optional>

namespace llarp::dns
{
  /// decode name from buffer; return nullopt on failure
  std::optional<std::string>
  DecodeName(llarp_buffer_t* buf, bool trimTrailingDot = false);

  /// encode name to buffer
  bool
  EncodeNameTo(llarp_buffer_t* buf, std::string_view name);

  std::optional<huint128_t>
  DecodePTR(std::string_view name);

  bool
  NameIsReserved(std::string_view name);

}  // namespace llarp::dns
