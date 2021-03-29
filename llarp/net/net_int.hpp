#pragma once

// for addrinfo
#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#define inet_aton(x, y) inet_pton(AF_INET, x, y)
#endif

#include "net.h"

#include <cstdlib>  // for itoa
#include <iostream>
#include <llarp/util/endian.hpp>
#include <vector>

#include "uint128.hpp"

namespace llarp
{
  template <typename UInt_t>
  struct huint_t
  {
    UInt_t h;

    constexpr huint_t
    operator&(huint_t x) const
    {
      return huint_t{UInt_t{h & x.h}};
    }

    constexpr huint_t
    operator|(huint_t x) const
    {
      return huint_t{UInt_t{h | x.h}};
    }

    constexpr huint_t
    operator-(huint_t x) const
    {
      return huint_t{UInt_t{h - x.h}};
    }

    constexpr huint_t
    operator+(huint_t x) const
    {
      return huint_t{UInt_t{h + x.h}};
    }

    constexpr huint_t
    operator^(huint_t x) const
    {
      return huint_t{UInt_t{h ^ x.h}};
    }

    constexpr huint_t
    operator~() const
    {
      return huint_t{UInt_t{~h}};
    }

    constexpr huint_t
    operator<<(int n) const
    {
      UInt_t v{h};
      v <<= n;
      return huint_t{v};
    }

    inline huint_t
    operator++()
    {
      ++h;
      return *this;
    }

    inline huint_t
    operator--()
    {
      --h;
      return *this;
    }

    constexpr bool
    operator<(huint_t x) const
    {
      return h < x.h;
    }

    constexpr bool
    operator!=(huint_t x) const
    {
      return h != x.h;
    }

    constexpr bool
    operator==(huint_t x) const
    {
      return h == x.h;
    }

    using V6Container = std::vector<uint8_t>;
    void
    ToV6(V6Container& c);

    std::string
    ToString() const;

    bool
    FromString(const std::string&);

    friend std::ostream&
    operator<<(std::ostream& out, const huint_t& i)
    {
      return out << i.ToString();
    }
  };

  using huint32_t = huint_t<uint32_t>;
  using huint16_t = huint_t<uint16_t>;
  using huint128_t = huint_t<llarp::uint128_t>;

  template <typename UInt_t>
  struct nuint_t
  {
    UInt_t n = 0;

    constexpr nuint_t
    operator&(nuint_t x) const
    {
      return nuint_t{UInt_t(n & x.n)};
    }

    constexpr nuint_t
    operator|(nuint_t x) const
    {
      return nuint_t{UInt_t(n | x.n)};
    }

    constexpr nuint_t
    operator^(nuint_t x) const
    {
      return nuint_t{UInt_t(n ^ x.n)};
    }

    constexpr nuint_t
    operator~() const
    {
      return nuint_t{UInt_t(~n)};
    }

    inline nuint_t
    operator++()
    {
      ++n;
      return *this;
    }
    inline nuint_t
    operator--()
    {
      --n;
      return *this;
    }

    constexpr bool
    operator<(nuint_t x) const
    {
      return n < x.n;
    }

    constexpr bool
    operator==(nuint_t x) const
    {
      return n == x.n;
    }

    using V6Container = std::vector<uint8_t>;
    void
    ToV6(V6Container& c);

    std::string
    ToString() const;

    bool
    FromString(const std::string& data)
    {
      huint_t<UInt_t> x;
      if (not x.FromString(data))
        return false;
      *this = ToNet(x);
      return true;
    }

    friend std::ostream&
    operator<<(std::ostream& out, const nuint_t& i)
    {
      return out << i.ToString();
    }
  };

  using nuint32_t = nuint_t<uint32_t>;
  using nuint16_t = nuint_t<uint16_t>;
  using nuint128_t = nuint_t<llarp::uint128_t>;

  static inline nuint32_t
  xhtonl(huint32_t x)
  {
    return nuint32_t{htonl(x.h)};
  }

  static inline huint32_t
  xntohl(nuint32_t x)
  {
    return huint32_t{ntohl(x.n)};
  }

  static inline nuint16_t
  xhtons(huint16_t x)
  {
    return nuint16_t{htons(x.h)};
  }

  static inline huint16_t
  xntohs(nuint16_t x)
  {
    return huint16_t{ntohs(x.n)};
  }

  huint16_t ToHost(nuint16_t);
  huint32_t ToHost(nuint32_t);
  huint128_t ToHost(nuint128_t);

  nuint16_t ToNet(huint16_t);
  nuint32_t ToNet(huint32_t);
  nuint128_t ToNet(huint128_t);
}  // namespace llarp

namespace std
{
  template <typename UInt_t>
  struct hash<llarp::nuint_t<UInt_t>>
  {
    size_t
    operator()(const llarp::nuint_t<UInt_t>& x) const
    {
      return std::hash<UInt_t>{}(x.n);
    }
  };

  template <typename UInt_t>
  struct hash<llarp::huint_t<UInt_t>>
  {
    size_t
    operator()(const llarp::huint_t<UInt_t>& x) const
    {
      return std::hash<UInt_t>{}(x.h);
    }
  };
}  // namespace std
