#ifndef LLARP_NET_ADDR_HPP
#define LLARP_NET_ADDR_HPP

#include <net/address_info.hpp>
#include <net/net.h>
#include <net/net.hpp>
#include <net/net_int.hpp>

#include <string_view>
#include <string>

namespace llarp
{
  // real work
  struct Addr
  {
    // network order
    sockaddr_in6 _addr;
    sockaddr_in _addr4;
    ~Addr();

    Addr();

    Addr(std::string_view str);

    Addr(std::string_view str, const uint16_t p_port);

    Addr(std::string_view addr_str, std::string_view port_str);

    void
    port(uint16_t port);

    in6_addr*
    addr6();

    in_addr*
    addr4();

    const in6_addr*
    addr6() const;

    const in_addr*
    addr4() const;

    bool
    FromString(std::string_view str);

    bool
    from_4int(const uint8_t one, const uint8_t two, const uint8_t three, const uint8_t four);

    Addr(const uint8_t one, const uint8_t two, const uint8_t three, const uint8_t four);

    Addr(
        const uint8_t one,
        const uint8_t two,
        const uint8_t three,
        const uint8_t four,
        const uint16_t p_port);

    Addr(const AddressInfo& other);
    Addr(const sockaddr_in& other);
    Addr(const sockaddr_in6& other);
    Addr(const sockaddr& other);

    std::string
    ToString() const;

    friend std::ostream&
    operator<<(std::ostream& out, const Addr& a);

    operator const sockaddr*() const;

    operator sockaddr*() const;

    void
    CopyInto(sockaddr* other) const;

    int
    af() const;

    uint16_t
    port() const;

    bool
    operator<(const Addr& other) const;

    bool
    operator==(const Addr& other) const;

    Addr&
    operator=(const sockaddr& other);

    inline uint32_t
    tohl() const
    {
      return ntohl(addr4()->s_addr);
    }

    inline huint32_t
    xtohl() const
    {
      return huint32_t{ntohl(addr4()->s_addr)};
    }

    inline uint32_t
    ton() const
    {
      return addr4()->s_addr;
    }

    inline nuint32_t
    xtonl() const
    {
      return nuint32_t{addr4()->s_addr};
    }

    bool
    sameAddr(const Addr& other) const;

    bool
    operator!=(const Addr& other) const;

    inline uint32_t
    getHostLong()
    {
      in_addr_t addr = this->addr4()->s_addr;
      uint32_t byte = ntohl(addr);
      return byte;
    }

    bool
    isTenPrivate(uint32_t byte);

    bool
    isOneSevenPrivate(uint32_t byte);

    bool
    isOneNinePrivate(uint32_t byte);

    /// return true if our ipv4 address is a bogon
    /// TODO: ipv6
    bool
    IsBogon() const;

    socklen_t
    SockLen() const;

    bool
    isPrivate() const;

    bool
    isLoopback() const;

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
    };
  };  // end struct
}  // namespace llarp
#endif
