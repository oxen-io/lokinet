#pragma once

#include <string_view>
#include <string>

namespace llarp
{
  /// A simple SockAddr wrapper which provides a sockaddr_in (IPv4). Memory management is handled
  /// in constructor and destructor (if needed) and copying is disabled.
  ///
  /// This is immutable.
  struct SockAddr
  {
    sockaddr_in _addr4;

    SockAddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
    SockAddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port);
    SockAddr(uint32_t addr);
    SockAddr(std::string_view addr);
  };
}  // namespace llarp
