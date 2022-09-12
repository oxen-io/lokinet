#pragma once

#include <memory>

namespace llarp
{
  struct AbstractRouter;
}

namespace llarp::vpn
{
  struct InterfaceInfo;
  class NetworkInterface;
}  // namespace llarp::vpn

namespace llarp::win32::wintun
{
  /// makes a new vpn interface with a wintun context given info and a router pointer
  std::shared_ptr<vpn::NetworkInterface>
  make_interface(
      const vpn::InterfaceInfo& info,
      AbstractRouter* router);

}  // namespace llarp::win32
