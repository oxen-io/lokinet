#pragma once

#include <net/sock_addr.hpp>

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
  ///
  /// TODO: IPv6 is not currently supported.
  struct IpAddress
  {
    /// Constructor. Takes a string which can be an IPv4 or IPv6 address optionally followed by
    /// a colon and a port.
    ///
    /// Examples:
    ///
    /// 127.0.0.1
    /// 1.2.3.4:53
    ///
    /// Note that an IPv6 + port representation must be done in an unambiguous way, e.g. wrap the
    /// IP portion of the string in brackets: [IPv6Addr]:port
    ///
    /// @param str is a string representing an IP address and optionally a port
    /// @throws std::invalid_argument if str cannot be parsed
    IpAddress(std::string_view str);

    /// Constructor. Takes an IP address (as above) and a port. The string may not contain a port.
    ///
    /// @param str is a string representing an IP address and optionally a port
    /// @throws std::invalid_argument if str cannot be parsed
    IpAddress(std::string_view str, std::optional<uint16_t> port);

    /// Return the port. Returns -1 if no port has been provided.
    ///
    /// @return the port, if present
    std::optional<uint16_t>
    getPort() const;

    /// Set the port.
    ///
    /// @param port
    void
    setPort(std::optional<uint16_t> port);

    /// Returns true if this is an IPv4 address (or an IPv6 address representing an IPv4 address)
    ///
    /// TODO: could return an int (e.g. 4 or 6) or an enum
    ///
    /// @return true if this is an IPv4 address, false otherwise
    bool
    isIPv4();

    /// Creates an instance of SockAddr representing this IpAddress.
    ///
    /// @return an instance of a SockAddr created from this IpAddress
    SockAddr
    createSockAddr() const;

    // TODO: other utility functions left over from Addr which may be useful
    // IsBogon() const;
    // isPrivate() const;
    // struct Hash
    // operator<(const Addr& other) const;
    // operator==(const Addr& other) const;
    // operator=(const sockaddr& other);

   private:
    std::string m_ipAddress;
    std::optional<uint16_t> m_port;
  };
}  // namespace llarp
