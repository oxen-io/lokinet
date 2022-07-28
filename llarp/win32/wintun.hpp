#pragma once

#include <memory>

#include <windows.h>

namespace llarp
{
  struct AbstractRouter;
}

namespace llarp::vpn
{
  struct InterfaceInfo;
  class NetworkInterface;
}  // namespace llarp::vpn

namespace llarp::win32
{
  /// holds all wintun implementation, including function pointers we fetch out of the wintun
  /// library forward declared to hide wintun from other compilation units
  class WintunContext;

  std::shared_ptr<WintunContext>
  WintunContext_new();

  /// makes a new vpn interface with a wintun context given info and a router pointer
  std::shared_ptr<vpn::NetworkInterface>
  WintunInterface_new(
      const std::shared_ptr<WintunContext>&,
      const vpn::InterfaceInfo& info,
      AbstractRouter* router);

}  // namespace llarp::win32
