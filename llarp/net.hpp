#ifndef LLARP_NET_HPP
#define LLARP_NET_HPP
#include <llarp/address_info.h>
#include <llarp/net.h>
#include <string>
#include "mem.hpp"

bool
operator==(const sockaddr& a, const sockaddr& b);

bool
operator<(const sockaddr_in6& a, const sockaddr_in6& b);

bool
operator<(const in6_addr& a, const in6_addr& b);

namespace llarp
{
  struct Addr
  {
    sockaddr_in6 _addr;

    ~Addr(){};

    Addr(){};

    Addr(const Addr& other)
    {
      memcpy(&_addr, &other._addr, sizeof(sockaddr_in6));
    }

    in6_addr*
    addr6()
    {
      return (in6_addr*)&_addr.sin6_addr.s6_addr[0];
    }

    in_addr*
    addr4()
    {
      return (in_addr*)&_addr.sin6_addr.s6_addr[12];
    }

    const in6_addr*
    addr6() const
    {
      return (const in6_addr*)&_addr.sin6_addr.s6_addr[0];
    }

    const in_addr*
    addr4() const
    {
      return (const in_addr*)&_addr.sin6_addr.s6_addr[12];
    }

    Addr(const llarp_ai& other)
    {
      _addr.sin6_family = AF_INET6;
      memcpy(addr6(), other.ip.s6_addr, 16);
      _addr.sin6_port = htons(other.port);
    }

    Addr(const sockaddr& other)
    {
      llarp::Zero(&_addr, sizeof(sockaddr_in6));
      _addr.sin6_family = other.sa_family;
      uint8_t* addrptr  = _addr.sin6_addr.s6_addr;
      uint16_t* port    = &_addr.sin6_port;
      switch(other.sa_family)
      {
        case AF_INET:
          // SIIT
          memcpy(12 + addrptr, &((const sockaddr_in*)(&other))->sin_addr,
                 sizeof(in_addr));
          addrptr[11] = 0xff;
          addrptr[10] = 0xff;
          *port       = ((sockaddr_in*)(&other))->sin_port;
          break;
        case AF_INET6:
          memcpy(addrptr, &((const sockaddr_in6*)(&other))->sin6_addr.s6_addr,
                 16);
          *port = ((sockaddr_in6*)(&other))->sin6_port;
          break;
          // TODO : sockaddr_ll
        default:
          break;
      }
    }

    std::string
    to_string() const
    {
      std::string str;
      char tmp[128];
      socklen_t sz;
      const void* ptr = nullptr;
      if(af() == AF_INET6)
      {
        str += "[";
        sz  = sizeof(sockaddr_in6);
        ptr = addr6();
      }
      else
      {
        sz  = sizeof(sockaddr_in);
        ptr = addr4();
      }
      if(inet_ntop(af(), ptr, tmp, sz))
      {
        str += tmp;
        if(af() == AF_INET6)
          str += "]";
      }

      return str + ":" + std::to_string(port());
    }

    operator const sockaddr*() const
    {
      return (const sockaddr*)&_addr;
    }

    void
    CopyInto(sockaddr* other) const
    {
      void *dst, *src;
      in_port_t* ptr;
      size_t slen;
      switch(af())
      {
        case AF_INET:
          dst  = (void*)&((sockaddr_in*)other)->sin_addr.s_addr;
          src  = (void*)&_addr.sin6_addr.s6_addr[12];
          ptr  = &((sockaddr_in*)other)->sin_port;
          slen = sizeof(in_addr);
          break;
        case AF_INET6:
          dst  = (void*)((sockaddr_in6*)other)->sin6_addr.s6_addr;
          src  = (void*)_addr.sin6_addr.s6_addr;
          ptr  = &((sockaddr_in6*)other)->sin6_port;
          slen = sizeof(in6_addr);
          break;
        default:
          return;
      }
      memcpy(dst, src, slen);
      *ptr             = htons(port());
      other->sa_family = af();
    }

    int
    af() const
    {
      return _addr.sin6_family;
    }

    uint16_t
    port() const
    {
      return ntohs(_addr.sin6_port);
    }

    bool
    operator<(const Addr& other) const
    {
      return af() < other.af() && *addr6() < *other.addr6()
          && port() < other.port();
    }
  };
}

#endif
