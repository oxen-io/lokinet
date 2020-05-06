#pragma once

#include <netinet/in.h>

#include <string_view>
#include <string>

namespace llarp
{
  /// A simple SockAddr wrapper which provides a sockaddr_in (IPv4). Memory management is handled
  /// in constructor and destructor (if needed) and copying is disabled.
  ///
  /// This is immutable other than assignment operator.
  struct SockAddr
  {
    SockAddr();
    SockAddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
    SockAddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port);
    SockAddr(uint32_t addr);
    SockAddr(std::string_view addr);

    SockAddr(const SockAddr&);
    SockAddr&
    operator=(const SockAddr&) const;

    SockAddr(const sockaddr* addr);
    SockAddr(const sockaddr& addr);
    SockAddr(const sockaddr_in* addr);
    SockAddr(const sockaddr_in& addr);
    operator const sockaddr*() const;

    bool
    isEmpty() const;

   private:
    sockaddr_in addr;
  };

  std::ostream&
  operator<<(std::ostream& out, const SockAddr& address);

}  // namespace llarp
