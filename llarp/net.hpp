#ifndef LLARP_NET_HPP
#define LLARP_NET_HPP
#include <llarp/address_info.h>
#include <llarp/net.h>
#include <string>

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
    int af        = AF_INET;
    in6_addr addr = {};
    uint16_t port = 0;

    sockaddr _addr = {0, {0}};

    ~Addr(){};

    Addr(){};

    Addr(const Addr& other)
    {
      af   = other.af;
      port = other.port;
      memcpy(addr.s6_addr, other.addr.s6_addr, sizeof(addr.s6_addr));
      CopyInto(_addr);
    }

    Addr(const llarp_ai& other)
    {
      af = AF_INET6;
      memcpy(addr.s6_addr, other.ip.s6_addr, 16);
      port = other.port;
      CopyInto(_addr);
    }

    Addr(const sockaddr& other)
    {
      uint8_t* addrptr = addr.s6_addr;
      switch(other.sa_family)
      {
        case AF_INET:
          // SIIT
          af = AF_INET;
          memcpy(12 + addrptr, &((const sockaddr_in*)(&other))->sin_addr,
                 sizeof(in_addr));
          addrptr[11] = 0xff;
          addrptr[10] = 0xff;
          port        = ntohs(((sockaddr_in*)(&other))->sin_port);
          break;
        case AF_INET6:
          af = AF_INET6;
          memcpy(addrptr, &((const sockaddr_in6*)(&other))->sin6_addr,
                 sizeof(addr.s6_addr));
          port = ntohs(((sockaddr_in6*)(&other))->sin6_port);
          break;
          // TODO : sockaddr_ll
        default:
          break;
      }
      CopyInto(_addr);
    }

    std::string
    to_string() const
    {
      std::string str;
      char tmp[128];
      socklen_t sz;
      const void* ptr = nullptr;
      if(af == AF_INET)
      {
        sz  = sizeof(sockaddr_in);
        ptr = &addr.s6_addr[12];
      }
      if(af == AF_INET6)
      {
        str += "[";
        sz  = sizeof(sockaddr_in6);
        ptr = &addr.s6_addr[0];
      }
      if(inet_ntop(af, ptr, tmp, sz))
      {
        str += tmp;
        if(af == AF_INET6)
          str += "]";
      }

      return str + ":" + std::to_string(port);
    }

    operator const sockaddr*() const
    {
      return &_addr;
    }

    void
    CopyInto(sockaddr& other) const
    {
      void *dst, *src;
      in_port_t* ptr;
      size_t slen;
      switch(af)
      {
        case AF_INET:
          dst  = (void*)&((const sockaddr_in*)&other)->sin_addr;
          src  = (void*)&addr.s6_addr[12];
          ptr  = &((sockaddr_in*)&other)->sin_port;
          slen = sizeof(in_addr);
          break;
        case AF_INET6:
          dst  = (void*)&((const sockaddr_in6*)&other)->sin6_addr;
          src  = (void*)&addr.s6_addr[0];
          ptr  = &((sockaddr_in6*)&other)->sin6_port;
          slen = sizeof(in6_addr);
          break;
        default:
          return;
      }
      memcpy(ptr, src, slen);
      *ptr            = htons(port);
      other.sa_family = af;
    }

    bool
    operator<(const Addr& other) const
    {
      return af < other.af && addr < other.addr && port < other.port;
    }
  };
}

#endif
