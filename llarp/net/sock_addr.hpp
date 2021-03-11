#pragma once

#ifndef _WIN32
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>
extern "C" const char*
inet_ntop(int af, const void* src, char* dst, size_t size);
extern "C" int
inet_pton(int af, const char* src, void* dst);
#define inet_aton(x, y) inet_pton(AF_INET, x, y)
#endif

#include <string_view>
#include <string>
#include <net/net_int.hpp>

namespace llarp
{
  struct AddressInfo;

  /// A simple SockAddr wrapper which provides a sockaddr_in (IPv4). Memory management is handled
  /// in constructor and destructor (if needed) and copying is disabled.
  struct SockAddr
  {
    SockAddr();
    SockAddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
    SockAddr(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port);
    SockAddr(std::string_view addr);
    SockAddr(std::string_view addr, uint16_t port);
    SockAddr(uint32_t ip, uint16_t port);

    SockAddr(const AddressInfo&);

    SockAddr(const SockAddr&);
    SockAddr&
    operator=(const SockAddr&);

    SockAddr(const sockaddr& addr);
    SockAddr&
    operator=(const sockaddr& addr);

    SockAddr(const sockaddr_in& addr);
    SockAddr&
    operator=(const sockaddr_in& addr);

    SockAddr(const sockaddr_in6& addr);
    SockAddr&
    operator=(const sockaddr_in6& addr);

    SockAddr(const in6_addr& addr);
    SockAddr&
    operator=(const in6_addr& addr);

    operator const sockaddr*() const;
    operator const sockaddr_in*() const;
    operator const sockaddr_in6*() const;

    bool
    operator<(const SockAddr& other) const;

    bool
    operator==(const SockAddr& other) const;

    void
    fromString(std::string_view str, bool allow_port = true);

    std::string
    toString() const;

    /// Returns true if this is an empty SockAddr, defined by having no IP address set. An empty IP
    /// address with a valid port is still considered empty.
    ///
    /// @return true if this is empty, false otherwise
    bool
    isEmpty() const;

    void
    setIPv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d);

    /// port is in host order
    void
    setIPv4(uint32_t ip);

    void
    setPort(uint16_t port);

    /// port is in host order
    uint16_t
    getPort() const;

    huint128_t
    asIPv6() const;
    /// in network order
    uint32_t
    getIPv4() const;

    huint32_t
    asIPv4() const;

    struct Hash
    {
      size_t
      operator()(const SockAddr& addr) const noexcept
      {
        const std::hash<uint16_t> port{};
        const std::hash<huint128_t> ip{};
        return (port(addr.getPort()) << 3) ^ ip(addr.asIPv6());
      }
    };

   private:
    bool m_empty = true;
    sockaddr_in6 m_addr;
    sockaddr_in m_addr4;

    void
    init();

    void
    applyIPv4MapBytes();
  };

  std::ostream&
  operator<<(std::ostream& out, const SockAddr& address);

}  // namespace llarp
