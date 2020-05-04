#include <net/net.hpp>
#include <net/net_addr.hpp>
#include <string_view>

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

namespace llarp
{
  Addr::Addr()
  {
    llarp::Zero(&_addr4, sizeof(_addr4));
    _addr4.sin_family = AF_INET;
    llarp::Zero(&_addr, sizeof(_addr));
    _addr.sin6_family = AF_INET6;
  }
  Addr::~Addr() = default;

  void
  Addr::port(uint16_t port)
  {
    _addr4.sin_port = htons(port);
    _addr.sin6_port = htons(port);
  }

  in6_addr*
  Addr::addr6()
  {
    return (in6_addr*)&_addr.sin6_addr.s6_addr[0];
  }

  in_addr*
  Addr::addr4()
  {
    return (in_addr*)&_addr.sin6_addr.s6_addr[12];
  }

  const in6_addr*
  Addr::addr6() const
  {
    return (const in6_addr*)&_addr.sin6_addr.s6_addr[0];
  }

  const in_addr*
  Addr::addr4() const
  {
    return (const in_addr*)&_addr.sin6_addr.s6_addr[12];
  }

  Addr::Addr(std::string_view str) : Addr()
  {
    this->FromString(str);
  }

  Addr::Addr(std::string_view str, const uint16_t p_port) : Addr(str)
  {
    this->port(p_port);
  }

  Addr::Addr(std::string_view addr_str, std::string_view port_str)
      : Addr(addr_str, std::strtoul(port_str.data(), nullptr, 10))
  {
  }

  bool
  Addr::FromString(std::string_view in)
  {
    // TODO: this will overwrite port, which may not be specified in the input string
    Zero(&_addr, sizeof(sockaddr_in6));

    std::string ipPortion;
    auto pPosition = in.find(':');
    if (pPosition != std::string_view::npos)
    {
      // parse port
      const std::string portStr = std::string(in.substr(pPosition + 1));
      uint16_t port = std::atoi(portStr.c_str());
      this->port(port);

      ipPortion = std::string(in.substr(0, pPosition));
    }
    else
    {
      ipPortion = std::string(in);
    }

    struct addrinfo hint, *res = nullptr;
    int ret;

    memset(&hint, '\0', sizeof hint);

    hint.ai_family = PF_UNSPEC;
    hint.ai_flags = AI_NUMERICHOST;

    if (pPosition != std::string_view::npos)
    {
      ret = getaddrinfo(
          std::string(in.begin(), in.begin() + pPosition).c_str(), nullptr, &hint, &res);
    }
    else
    {
      ret = getaddrinfo(std::string(in).c_str(), nullptr, &hint, &res);
    }

    if (ret)
    {
      LogError("failed to determine address family: ", in);
      return false;
    }

    if (res->ai_family == AF_INET6)
    {
      LogError("IPv6 address not supported yet", in);
      return false;
    }
    if (res->ai_family != AF_INET)
    {
      LogError("Address family not supported yet", in);
      return false;
    }

    // put it in _addr4
    struct in_addr* addr = &_addr4.sin_addr;
    if (inet_aton(ipPortion.c_str(), addr) == 0)
    {
      LogError("failed to parse ", in);
      return false;
    }

    _addr.sin6_family = res->ai_family;
    _addr4.sin_family = res->ai_family;
#if ((__APPLE__ && __MACH__) || __FreeBSD__)
    _addr4.sin_len = sizeof(in_addr);
#endif
    // set up SIIT
    uint8_t* addrptr = _addr.sin6_addr.s6_addr;
    addrptr[11] = 0xff;
    addrptr[10] = 0xff;
    memcpy(12 + addrptr, &addr->s_addr, sizeof(in_addr));
    freeaddrinfo(res);

    return true;
  }

  bool
  Addr::from_4int(const uint8_t one, const uint8_t two, const uint8_t three, const uint8_t four)
  {
    Zero(&_addr, sizeof(sockaddr_in6));
    struct in_addr* addr = &_addr4.sin_addr;
    auto* ip = (unsigned char*)&(addr->s_addr);

    _addr.sin6_family = AF_INET;  // set ipv4 mode
    _addr4.sin_family = AF_INET;
    _addr4.sin_port = 0;

#if ((__APPLE__ && __MACH__) || __FreeBSD__)
    _addr4.sin_len = sizeof(in_addr);
#endif
    // FIXME: watch endian
    ip[0] = one;
    ip[1] = two;
    ip[2] = three;
    ip[3] = four;
    // set up SIIT
    uint8_t* addrptr = _addr.sin6_addr.s6_addr;
    addrptr[11] = 0xff;
    addrptr[10] = 0xff;
    memcpy(12 + addrptr, &addr->s_addr, sizeof(in_addr));
    // copy ipv6 SIIT into _addr4
    memcpy(&_addr4.sin_addr.s_addr, addr4(), sizeof(in_addr));
    return true;
  }

  Addr::Addr(const uint8_t one, const uint8_t two, const uint8_t three, const uint8_t four) : Addr()
  {
    this->from_4int(one, two, three, four);
  }

  Addr::Addr(
      const uint8_t one,
      const uint8_t two,
      const uint8_t three,
      const uint8_t four,
      const uint16_t p_port)
      : Addr()
  {
    this->from_4int(one, two, three, four);
    this->port(p_port);
  }

  Addr::Addr(const AddressInfo& other) : Addr()
  {
    memcpy(addr6(), other.ip.s6_addr, 16);
    _addr.sin6_port = htons(other.port);
    if (ipv6_is_siit(other.ip))
    {
      _addr4.sin_family = AF_INET;
      _addr4.sin_port = htons(other.port);
      _addr.sin6_family = AF_INET;
      memcpy(&_addr4.sin_addr.s_addr, addr4(), sizeof(in_addr));
    }
    else
      _addr.sin6_family = AF_INET6;
  }

  Addr::Addr(const sockaddr_in& other) : Addr()
  {
    Zero(&_addr, sizeof(sockaddr_in6));
    _addr.sin6_family = AF_INET;
    uint8_t* addrptr = _addr.sin6_addr.s6_addr;
    uint16_t* port = &_addr.sin6_port;
    // SIIT
    memcpy(12 + addrptr, &((const sockaddr_in*)(&other))->sin_addr, sizeof(in_addr));
    addrptr[11] = 0xff;
    addrptr[10] = 0xff;
    *port = ((sockaddr_in*)(&other))->sin_port;
    _addr4.sin_family = AF_INET;
    _addr4.sin_port = *port;
    memcpy(&_addr4.sin_addr.s_addr, addr4(), sizeof(in_addr));
  }

  Addr::Addr(const sockaddr_in6& other) : Addr()
  {
    memcpy(addr6(), other.sin6_addr.s6_addr, 16);
    _addr.sin6_port = htons(other.sin6_port);
    auto ptr = &_addr.sin6_addr.s6_addr[0];
    // TODO: detect SIIT better
    if (ptr[11] == 0xff && ptr[10] == 0xff && ptr[9] == 0 && ptr[8] == 0 && ptr[7] == 0
        && ptr[6] == 0 && ptr[5] == 0 && ptr[4] == 0 && ptr[3] == 0 && ptr[2] == 0 && ptr[1] == 0
        && ptr[0] == 0)
    {
      _addr4.sin_family = AF_INET;
      _addr4.sin_port = htons(other.sin6_port);
      _addr.sin6_family = AF_INET;
      memcpy(&_addr4.sin_addr.s_addr, addr4(), sizeof(in_addr));
    }
    else
      _addr.sin6_family = AF_INET6;
  }

  Addr::Addr(const sockaddr& other) : Addr()
  {
    Zero(&_addr, sizeof(sockaddr_in6));
    _addr.sin6_family = other.sa_family;
    uint8_t* addrptr = _addr.sin6_addr.s6_addr;
    uint16_t* port = &_addr.sin6_port;
    switch (other.sa_family)
    {
      case AF_INET:
        // SIIT
        memcpy(12 + addrptr, &((const sockaddr_in*)(&other))->sin_addr, sizeof(in_addr));
        addrptr[11] = 0xff;
        addrptr[10] = 0xff;
        *port = ((sockaddr_in*)(&other))->sin_port;
        _addr4.sin_family = AF_INET;
        _addr4.sin_port = *port;
        memcpy(&_addr4.sin_addr.s_addr, addr4(), sizeof(in_addr));
        break;
      case AF_INET6:
        memcpy(addrptr, &((const sockaddr_in6*)(&other))->sin6_addr.s6_addr, 16);
        *port = ((sockaddr_in6*)(&other))->sin6_port;
        break;
      // TODO : sockaddr_ll
      default:
        break;
    }
  }

  std::string
  Addr::ToString() const
  {
    std::stringstream ss;
    ss << (*this);
    return std::string(ss.str().c_str());
  }

  std::ostream&
  operator<<(std::ostream& out, const Addr& a)
  {
    char tmp[128] = {0};
    const void* ptr = nullptr;
    if (a.af() == AF_INET6)
    {
      out << "[";
      ptr = a.addr6();
    }
    else
    {
      ptr = a.addr4();
    }
    if (inet_ntop(a.af(), (void*)ptr, tmp, sizeof(tmp)))
    {
      out << tmp;
      if (a.af() == AF_INET6)
        out << "]";
    }
    return out << ":" << a.port();
  }

  void
  Addr::CopyInto(sockaddr* other) const
  {
    void *dst, *src;
    uint16_t* ptr;
    size_t slen;
    switch (af())
    {
      case AF_INET:
      {
        auto* ipv4_dst = (sockaddr_in*)other;
        dst = (void*)&ipv4_dst->sin_addr.s_addr;
        src = (void*)&_addr4.sin_addr.s_addr;
        ptr = &((sockaddr_in*)other)->sin_port;
        slen = sizeof(in_addr);
        break;
      }
      case AF_INET6:
      {
        dst = (void*)((sockaddr_in6*)other)->sin6_addr.s6_addr;
        src = (void*)_addr.sin6_addr.s6_addr;
        ptr = &((sockaddr_in6*)other)->sin6_port;
        slen = sizeof(in6_addr);
        break;
      }
      default:
      {
        return;
      }
    }
    memcpy(dst, src, slen);
    *ptr = htons(port());
    other->sa_family = af();
  }

  int
  Addr::af() const
  {
    return _addr.sin6_family;
  }

  uint16_t
  Addr::port() const
  {
    return ntohs(_addr.sin6_port);
  }

  Addr::operator const sockaddr*() const
  {
    if (af() == AF_INET)
      return (const sockaddr*)&_addr4;

    return (const sockaddr*)&_addr;
  }

  Addr::operator sockaddr*() const
  {
    if (af() == AF_INET)
      return (sockaddr*)&_addr4;

    return (sockaddr*)&_addr;
  }

  bool
  Addr::operator<(const Addr& other) const
  {
    if (af() == AF_INET && other.af() == AF_INET)
      return port() < other.port() || addr4()->s_addr < other.addr4()->s_addr;

    return port() < other.port() || *addr6() < *other.addr6() || af() < other.af();
  }

  bool
  Addr::operator==(const Addr& other) const
  {
    if (af() != other.af() || port() != other.port())
      return false;

    if (af() == AF_INET)
      return addr4()->s_addr == other.addr4()->s_addr;

    return memcmp(addr6(), other.addr6(), 16) == 0;
  }

  Addr&
  Addr::operator=(const sockaddr& other)
  {
    Zero(&_addr, sizeof(sockaddr_in6));
    _addr.sin6_family = other.sa_family;
    uint8_t* addrptr = _addr.sin6_addr.s6_addr;
    uint16_t* port = &_addr.sin6_port;
    switch (other.sa_family)
    {
      case AF_INET:
        // SIIT
        memcpy(12 + addrptr, &((const sockaddr_in*)(&other))->sin_addr, sizeof(in_addr));
        addrptr[11] = 0xff;
        addrptr[10] = 0xff;
        *port = ((sockaddr_in*)(&other))->sin_port;
        _addr4.sin_family = AF_INET;
        _addr4.sin_port = *port;
        memcpy(&_addr4.sin_addr.s_addr, addr4(), sizeof(in_addr));
        break;
      case AF_INET6:
        memcpy(addrptr, &((const sockaddr_in6*)(&other))->sin6_addr.s6_addr, 16);
        *port = ((sockaddr_in6*)(&other))->sin6_port;
        break;
      // TODO : sockaddr_ll
      default:
        break;
    }
    return *this;
  }

  bool
  Addr::sameAddr(const Addr& other) const
  {
    return memcmp(addr6(), other.addr6(), 16) == 0;
  }

  bool
  Addr::operator!=(const Addr& other) const
  {
    return !(*this == other);
  }

  bool
  Addr::isTenPrivate(uint32_t byte)
  {
    uint8_t byte1 = byte >> 24 & 0xff;
    return byte1 == 10;
  }

  bool
  Addr::isOneSevenPrivate(uint32_t byte)
  {
    uint8_t byte1 = byte >> 24 & 0xff;
    uint8_t byte2 = (0x00ff0000 & byte) >> 16;
    return byte1 == 172 && (byte2 >= 16 || byte2 <= 31);
  }

  bool
  Addr::isOneNinePrivate(uint32_t byte)
  {
    uint8_t byte1 = byte >> 24 & 0xff;
    uint8_t byte2 = (0x00ff0000 & byte) >> 16;
    return byte1 == 192 && byte2 == 168;
  }

  /// return true if our ipv4 address is a bogon
  /// TODO: ipv6
  bool
  Addr::IsBogon() const
  {
    return IsIPv4Bogon(xtohl());
  }

  socklen_t
  Addr::SockLen() const
  {
    if (af() == AF_INET)
      return sizeof(sockaddr_in);

    return sizeof(sockaddr_in6);
  }

  bool
  Addr::isPrivate() const
  {
    return IsBogon();
  }

  bool
  Addr::isLoopback() const
  {
    return (ntohl(addr4()->s_addr)) >> 24 == 127;
  }

  struct Hash
  {
    std::size_t
    operator()(Addr const& a) const noexcept
    {
      if (a.af() == AF_INET)
      {
        return a.port() ^ a.addr4()->s_addr;
      }
      static const uint8_t empty[16] = {0};
      return (a.af() + memcmp(a.addr6(), empty, 16)) ^ a.port();
    }
  };  // end struct Hash
}  // namespace llarp
