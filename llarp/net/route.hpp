#pragma once

#include <string>
#include <vector>

namespace llarp::net
{
  /// get every ip address that is a gateway that isn't owned by interface with name ifname
  std::vector<std::string>
  GetGatewaysNotOnInterface(std::string ifname);

  /// add route to ipaddr via gateway ip
  void
  AddRoute(std::string ipaddr, std::string gateway);

  /// delete route to ipaddr via gateway ip
  void
  DelRoute(std::string ipaddr, std::string gateway);

  /// add default route via interface with name ifname
  void
  AddDefaultRouteViaInterface(std::string ifname);

  /// delete default route via interface with name ifname
  void
  DelDefaultRouteViaInterface(std::string ifname);

  /// add route blackhole for all traffic
  void
  AddBlackhole();

  /// delete route blackhole for all traffic
  void
  DelBlackhole();

}  // namespace llarp::net
