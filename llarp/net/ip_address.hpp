#pragma once

#include <string_view>
#include <string>

namespace llarp
{
  /// A struct that can represent either an IPv4 or IPv6 address. It is meant for representation
  /// purposes only (e.g. serialization/deserialization). In addition, it also optionally stores
  /// a port.
  ///
  /// As a convenience, it can produce a SockAddr for dealing with network libraries which depend
  /// sockaddr structs. However, it does not keep this as a member variable and isn't responsible
  /// for its lifetime/memory/etc.
  struct IpAddress
  {
    /// Constructor. Takes a string which can be an IPv4 or IPv6 address optionally followed by
    /// a colon and a port.
    IpAddress(std::string_view str);

    /// Constructor. Takes an IP address (as above) and a port. The string may not contain a port.
    IpAddress(std::string_view str, int32_t port);

    /// Return the port. Returns -1 if no port has been provided.
    int32_t
    getPort() const;

    /// Set the port.
    void
    setPort(uint16_t port);

    /// Returns true if this is an IPv4 address (or an IPv6 address representing an IPv4 address),
    /// false otherwise.
    /// TODO: could return an int (e.g. 4 or 6) or an enum
    bool
    isIPv4();

    /// Creates an instance of SockAddr representing this IpAddress.
    SockAddr
    createSockAddr() const;

    // TODO: other utility functions left over from Addr which may be useful
    // isTenPrivate(uint32_t byte);
    // isOneSevenPrivate(uint32_t byte);
    // isOneNinePrivate(uint32_t byte);
    // IsBogon() const;
    // isPrivate() const;
    // struct Hash
    // operator<(const Addr& other) const;
    // operator==(const Addr& other) const;
    // operator=(const sockaddr& other);
  };
}  // namespace llarp
