#pragma once

#include "uint128.hpp"
#include "address_info.hpp"
#include "ip_address.hpp"
#include "net_int.hpp"
#include "net.h"
#include "ip_range.hpp"
#include <llarp/util/mem.hpp>
#include <llarp/util/bits.hpp>

#include <functional>
#include <cstdlib>  // for itoa
#include <vector>

// for addrinfo
#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>
#define inet_aton(x, y) inet_pton(AF_INET, x, y)
#endif

#ifndef _WIN32
#include <arpa/inet.h>
#endif

bool
operator==(const sockaddr& a, const sockaddr& b);

bool
operator==(const sockaddr_in& a, const sockaddr_in& b);

bool
operator==(const sockaddr_in6& a, const sockaddr_in6& b);

bool
operator<(const sockaddr_in6& a, const sockaddr_in6& b);

bool
operator<(const in6_addr& a, const in6_addr& b);

bool
operator==(const in6_addr& a, const in6_addr& b);

namespace llarp
{
  bool
  IsIPv4Bogon(const huint32_t& addr);

  inline bool
  IsIPv4Bogon(const nuint32_t& addr)
  {
    return IsIPv4Bogon(ToHost(addr));
  }

  bool
  IsBogon(const in6_addr& addr);

  bool
  IsBogon(const huint128_t addr);

  bool
  IsBogonRange(const in6_addr& host, const in6_addr& mask);

  namespace net
  {
    /// network platform (all methods virtual so it can be mocked by unit tests)
    class Platform
    {
     public:
      Platform() = default;
      virtual ~Platform() = default;
      Platform(const Platform&) = delete;
      Platform(Platform&&) = delete;

      /// get a pointer to our signleton instance used by main lokinet
      /// unit test mocks will not call this
      static const Platform*
      Default_ptr();

      virtual std::optional<SockAddr>
      AllInterfaces(SockAddr pubaddr) const = 0;

      virtual SockAddr
      Wildcard(int af = AF_INET) const = 0;

      inline SockAddr
      WildcardWithPort(port_t port, int af = AF_INET) const
      {
        auto addr = Wildcard(af);
        addr.setPort(port);
        return addr;
      }

      virtual std::string
      LoopbackInterfaceName() const = 0;

      virtual bool
      HasInterfaceAddress(ipaddr_t ip) const = 0;

      /// return true if ip is considered a loopback address
      virtual bool
      IsLoopbackAddress(ipaddr_t ip) const = 0;

      /// return true if ip is considered a wildcard address
      virtual bool
      IsWildcardAddress(ipaddr_t ip) const = 0;

      virtual std::optional<std::string>
      GetBestNetIF(int af = AF_INET) const = 0;

      inline std::optional<SockAddr>
      MaybeInferPublicAddr(port_t default_port, int af = AF_INET) const
      {
        std::optional<SockAddr> maybe_addr;
        if (auto maybe_ifname = GetBestNetIF(af))
          maybe_addr = GetInterfaceAddr(*maybe_ifname, af);

        if (maybe_addr)
          maybe_addr->setPort(default_port);
        return maybe_addr;
      }

      virtual std::optional<IPRange>
      FindFreeRange() const = 0;

      virtual std::optional<std::string>
      FindFreeTun() const = 0;

      virtual std::optional<SockAddr>
      GetInterfaceAddr(std::string_view ifname, int af = AF_INET) const = 0;

      inline std::optional<huint128_t>
      GetInterfaceIPv6Address(std::string_view ifname) const
      {
        if (auto maybe_addr = GetInterfaceAddr(ifname, AF_INET6))
          return maybe_addr->asIPv6();
        return std::nullopt;
      }

      virtual bool
      IsBogon(const SockAddr& addr) const = 0;

      virtual std::optional<int>
      GetInterfaceIndex(ipaddr_t ip) const = 0;
    };

  }  // namespace net

}  // namespace llarp
