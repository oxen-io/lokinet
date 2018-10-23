#ifndef LLARP_NET_INADDR_HPP
#define LLARP_NET_INADDR_HPP

#include <llarp/net.hpp>

namespace llarp
{
  /// IPv4 or IPv6 holder
  struct inAddr
  {
    // unsigned char   s6_addr[16];
    struct in6_addr _addr;  // store in network order

    void
    reset();

    bool
    from_char_array(const char* str);

    void
    fromSIIT();

    void
    toSIIT();

    inline bool
    isIPv6Mode() const;

    bool
    isIPv4Mode() const;

    void
    setIPv4Mode();
    void
    hexDebug();

    //
    // IPv4 specific functions
    //

    in_addr
    toIAddr();

    void
    from4int(const uint8_t one, const uint8_t two, const uint8_t three,
             const uint8_t four);

    void
    fromN32(nuint32_t in);
    void
    fromH32(huint32_t in);
    nuint32_t
    toN32();
    huint32_t
    toH32();

    //
    // IPv6 specific functions
    //
  };

}  // namespace llarp

#endif
