#ifndef LLARP_NET_HPP
#define LLARP_NET_HPP
#include <llarp/address_info.h>
#include <llarp/net.h>
#include <functional>
#include <iostream>
#include "logger.hpp"
#include "mem.hpp"

#include <stdlib.h>  // for itoa

bool
operator==(const sockaddr& a, const sockaddr& b);

bool
operator<(const sockaddr_in6& a, const sockaddr_in6& b);

bool
operator<(const in6_addr& a, const in6_addr& b);

struct privatesInUse
{
  bool ten;
  bool oneSeven;
  bool oneNine;
};

struct privatesInUse
llarp_getPrivateIfs();

namespace llarp
{
  struct Addr
  {
    sockaddr_in6 _addr;
    sockaddr_in _addr4;
    ~Addr(){};

    Addr(){};

    Addr(const Addr& other)
    {
      memcpy(&_addr, &other._addr, sizeof(sockaddr_in6));
      memcpy(&_addr4, &other._addr4, sizeof(sockaddr_in));
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

    Addr(const uint8_t one, const uint8_t two, const uint8_t three,
         const uint8_t four)
    {
      struct in_addr* addr = &_addr4.sin_addr;
      unsigned char* ip    = (unsigned char*)&(addr->s_addr);

      _addr.sin6_family = AF_INET;  // set ipv4 mode
      _addr4.sin_family = AF_INET;
      _addr4.sin_port   = htons(0);

#if((__APPLE__ && __MACH__) || __FreeBSD__)
      _addr4.sin_len = sizeof(in_addr);
#endif
      ip[0] = one;
      ip[1] = two;
      ip[2] = three;
      ip[3] = four;
    }

    Addr(const llarp_ai& other)
    {
      memcpy(addr6(), other.ip.s6_addr, 16);
      _addr.sin6_port = htons(other.port);
      auto ptr        = &_addr.sin6_addr.s6_addr[0];
      // TODO: detect SIIT better
      if(ptr[11] == 0xff && ptr[10] == 0xff && ptr[9] == 0 && ptr[8] == 0
         && ptr[7] == 0 && ptr[6] == 0 && ptr[5] == 0 && ptr[4] == 0
         && ptr[3] == 0 && ptr[2] == 0 && ptr[1] == 0 && ptr[0] == 0)
      {
        _addr4.sin_family = AF_INET;
        _addr4.sin_port   = htons(other.port);
        _addr.sin6_family = AF_INET;
        memcpy(&_addr4.sin_addr.s_addr, addr4(), sizeof(in_addr));
      }
      else
        _addr.sin6_family = AF_INET6;
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
          addrptr[11]       = 0xff;
          addrptr[10]       = 0xff;
          *port             = ((sockaddr_in*)(&other))->sin_port;
          _addr4.sin_family = AF_INET;
          _addr4.sin_port   = *port;
          memcpy(&_addr4.sin_addr.s_addr, addr4(), sizeof(in_addr));
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

    friend std::ostream&
    operator<<(std::ostream& out, const Addr& a)
    {
      char tmp[128] = {0};
      socklen_t sz;
      const void* ptr = nullptr;
      if(a.af() == AF_INET6)
      {
        out << "[";
        sz  = sizeof(sockaddr_in6);
        ptr = a.addr6();
      }
      else
      {
        sz  = sizeof(sockaddr_in);
        ptr = a.addr4();
      }
#ifndef _MSC_VER
      if(inet_ntop(a.af(), ptr, tmp, sz))
#else
      if(inet_ntop(a.af(), (void*)ptr, tmp, sz))
#endif
      {
        out << tmp;
        if(a.af() == AF_INET6)
          out << "]";
      }

      return out << ":" << a.port();
    }

    operator const sockaddr*() const
    {
      if(af() == AF_INET)
        return (const sockaddr*)&_addr4;
      else
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
        {
          sockaddr_in* ipv4_dst = (sockaddr_in*)other;
          dst                   = (void*)&ipv4_dst->sin_addr.s_addr;
          src                   = (void*)&_addr4.sin_addr.s_addr;
          ptr                   = &((sockaddr_in*)other)->sin_port;
          slen                  = sizeof(in_addr);
          break;
        }
        case AF_INET6:
        {
          dst  = (void*)((sockaddr_in6*)other)->sin6_addr.s6_addr;
          src  = (void*)_addr.sin6_addr.s6_addr;
          ptr  = &((sockaddr_in6*)other)->sin6_port;
          slen = sizeof(in6_addr);
          break;
        }
        default:
        {
          return;
        }
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
      if(af() == AF_INET && other.af() == AF_INET)
        return port() < other.port() || addr4()->s_addr < other.addr4()->s_addr;
      else
        return port() < other.port() || *addr6() < *other.addr6()
            || af() < other.af();
    }

    bool
    operator==(const Addr& other) const
    {
      if(af() == AF_INET && other.af() == AF_INET)
        return port() == other.port()
            && addr4()->s_addr == other.addr4()->s_addr;
      else
        return af() == other.af() && memcmp(addr6(), other.addr6(), 16) == 0
            && port() == other.port();
    }

    bool
    sameAddr(const Addr& other) const
    {
      return memcmp(addr6(), other.addr6(), 16) == 0;
    }

    bool
    operator!=(const Addr& other) const
    {
      return !(*this == other);
    }

    inline uint32_t
    getHostLong()
    {
      in_addr_t addr = this->addr4()->s_addr;
      uint32_t byte  = ntohl(addr);
      return byte;
    };

    bool
    isTenPrivate(uint32_t byte)
    {
      uint8_t byte1 = byte >> 24 & 0xff;
      return byte1 == 10;
    }

    bool
    isOneSevenPrivate(uint32_t byte)
    {
      uint8_t byte1 = byte >> 24 & 0xff;
      uint8_t byte2 = (0x00ff0000 & byte >> 16);
      return byte1 == 172 && (byte2 >= 16 || byte2 <= 31);
    }

    bool
    isOneNinePrivate(uint32_t byte)
    {
      uint8_t byte1 = byte >> 24 & 0xff;
      uint8_t byte2 = (0x00ff0000 & byte >> 16);
      return byte1 == 192 && byte2 == 168;
    }

    bool
    isPrivate()
    {
      uint32_t byte = getHostLong();
      return this->isTenPrivate(byte) || this->isOneSevenPrivate(byte)
          || this->isOneNinePrivate(byte);
    }

    bool
    isLoopback()
    {
      return (ntohl(addr4()->s_addr)) >> 24 == 127;
    }

    struct Hash
    {
      std::size_t
      operator()(Addr const& a) const noexcept
      {
        if(a.af() == AF_INET)
        {
          return a.port() ^ a.addr4()->s_addr;
        }
        uint8_t empty[16] = {0};
        return (a.af() + memcmp(a.addr6(), empty, 16)) ^ a.port();
      }
    };
  };

  /// get first network interface with public address
  bool
  GetBestNetIF(std::string& ifname, int af = AF_INET);

}  // namespace llarp

#endif
