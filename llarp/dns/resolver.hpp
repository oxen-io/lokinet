#pragma once
#include <string>
#include <llarp/net/sock_addr.hpp>

namespace llarp::dns
{
  /// Attempts to set lokinet as the DNS server for systemd-resolved.
  /// Returns true if successful, false if unsupported or fails.
  ///
  /// If systemd support is enabled it will attempt via dbus call to system-resolved
  /// When compiled without systemd support this always return false without doing anything.
  ///
  /// \param if_name -- the interface name to which we add the DNS servers, e.g. lokitun0.
  /// Typically tun_endpoint.GetIfName().
  /// \param dns -- the listening address of the lokinet DNS server
  /// \param global -- whether to set up lokinet for all DNS queries (true) or just .loki & .snode
  /// addresses (false).
  bool
  set_resolver(std::string if_name, llarp::SockAddr dns, bool global);

}  // namespace llarp::dns
