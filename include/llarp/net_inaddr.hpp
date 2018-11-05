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

    /// zero out
    void
    reset();

    /// from char*
    bool
    from_char_array(const char* str);

    /// convert from SIIT to IPv4 Mode
    void
    fromSIIT();

    /// convert from IPv4 Mode to SIIT
    void
    toSIIT();

    /// not IPv4 Mode (an actual IPv6 address)
    inline bool
    isIPv6Mode() const;

    /// IPv4 mode (not SIIT)
    bool
    isIPv4Mode() const;

    /// clear out bytes 5-15 (Last 12 bytes)
    /// This is how inet_pton works with IPv4 addresses
    void
    setIPv4Mode();

    /// make debugging/testing easier
    void
    hexDebug();

    //
    // IPv4 specific functions
    //

    /// make ipv4 in_addr struct
    in_addr
    toIAddr();

    /// set an IPv4 addr
    void
    from4int(const uint8_t one, const uint8_t two, const uint8_t three,
             const uint8_t four);

    /// set from an net-order uint32_t
    void
    fromN32(nuint32_t in);
    /// set from an host-order uint32_t
    void
    fromH32(huint32_t in);
    /// output as net-order uint32_t
    nuint32_t
    toN32();
    /// output as host-order uint32_t
    huint32_t
    toH32();

    //
    // IPv6 specific functions
    //
    // coming soon
  };

}  // namespace llarp

#endif
