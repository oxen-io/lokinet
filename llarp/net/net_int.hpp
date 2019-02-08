#ifndef LLARP_NET_INT_HPP
#define LLARP_NET_INT_HPP

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

#include <net/net.h>

#include <stdlib.h>  // for itoa
#include <iostream>

namespace llarp
{
  // clang-format off

  struct huint32_t
  {
    uint32_t h;

    constexpr huint32_t
    operator &(huint32_t x) const { return huint32_t{uint32_t(h & x.h)}; }
    constexpr huint32_t
    operator |(huint32_t x) const { return huint32_t{uint32_t(h | x.h)}; }
    constexpr huint32_t
    operator ^(huint32_t x) const { return huint32_t{uint32_t(h ^ x.h)}; }

    constexpr huint32_t
    operator ~() const { return huint32_t{uint32_t(~h)}; }

    inline huint32_t operator ++() { ++h; return *this; }
    inline huint32_t operator --() { --h; return *this; }

    constexpr bool operator <(huint32_t x) const { return h < x.h; }
    constexpr bool operator ==(huint32_t x) const { return h == x.h; }

    friend std::ostream&
    operator<<(std::ostream& out, const huint32_t& a)
    {
      uint32_t n = htonl(a.h);
      char tmp[INET_ADDRSTRLEN]   = {0};
      if(inet_ntop(AF_INET, (void*)&n, tmp, sizeof(tmp)))
      {
        out << tmp;
      }
      return out;
    }

    std::string ToString() const
    {
      uint32_t n = htonl(h);
      char tmp[INET_ADDRSTRLEN]   = {0};
      if(!inet_ntop(AF_INET, (void*)&n, tmp, sizeof(tmp)))
        return "";
      return tmp;
    }
    
    struct Hash
    {
      inline size_t
      operator ()(huint32_t x) const
      {
        return std::hash< uint32_t >{}(x.h);
      }
    };
  };

  struct nuint32_t
  {
    uint32_t n;

    constexpr nuint32_t
    operator &(nuint32_t x) const { return nuint32_t{uint32_t(n & x.n)}; }
    constexpr nuint32_t
    operator |(nuint32_t x) const { return nuint32_t{uint32_t(n | x.n)}; }
    constexpr nuint32_t
    operator ^(nuint32_t x) const { return nuint32_t{uint32_t(n ^ x.n)}; }

    constexpr nuint32_t
    operator ~() const { return nuint32_t{uint32_t(~n)}; }

    inline nuint32_t operator ++() { ++n; return *this; }
    inline nuint32_t operator --() { --n; return *this; }

    constexpr bool operator <(nuint32_t x) const { return n < x.n; }
    constexpr bool operator ==(nuint32_t x) const { return n == x.n; }

    friend std::ostream&
    operator<<(std::ostream& out, const nuint32_t& a)
    {
      char tmp[INET_ADDRSTRLEN]   = {0};
      if(inet_ntop(AF_INET, (void*)&a.n, tmp, sizeof(tmp)))
      {
        out << tmp;
      }
      return out;
    }

    struct Hash
    {
      inline size_t
      operator ()(nuint32_t x) const
      {
        return std::hash< uint32_t >{}(x.n);
      }
    };
  };

  struct huint16_t
  {
    uint16_t h;

    constexpr huint16_t
    operator &(huint16_t x) const { return huint16_t{uint16_t(h & x.h)}; }
    constexpr huint16_t
    operator |(huint16_t x) const { return huint16_t{uint16_t(h | x.h)}; }
    constexpr huint16_t
    operator ~() const { return huint16_t{uint16_t(~h)}; }

    inline huint16_t operator ++() { ++h; return *this; }
    inline huint16_t operator --() { --h; return *this; }

    constexpr bool operator <(huint16_t x) const { return h < x.h; }
    constexpr bool operator ==(huint16_t x) const { return h == x.h; }

    friend std::ostream&
    operator<<(std::ostream& out, const huint16_t& a)
    {
      return out << a.h;
    }

    struct Hash
    {
      inline size_t
      operator ()(huint16_t x) const
      {
        return std::hash< uint16_t >{}(x.h);
      }
    };
  };

  struct nuint16_t
  {
    uint16_t n;

    constexpr nuint16_t
    operator &(nuint16_t x) const { return nuint16_t{uint16_t(n & x.n)}; }
    constexpr nuint16_t
    operator |(nuint16_t x) const { return nuint16_t{uint16_t(n | x.n)}; }
    constexpr nuint16_t
    operator ~() const { return nuint16_t{uint16_t(~n)}; }

    inline nuint16_t operator ++() { ++n; return *this; }
    inline nuint16_t operator --() { --n; return *this; }

    constexpr bool operator <(nuint16_t x) const { return n < x.n; }
    constexpr bool operator ==(nuint16_t x) const { return n == x.n; }

    friend std::ostream&
    operator<<(std::ostream& out, const nuint16_t& a)
    {
      return out << ntohs(a.n);
    }

    struct Hash
    {
      inline size_t
      operator ()(nuint16_t x) const
      {
        return std::hash< uint16_t >{}(x.n);
      }
    };
  };

  // clang-format on

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
}  // namespace llarp

#endif
