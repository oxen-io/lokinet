#pragma once
#include <memory>
#include <string>
#include <windows.h>
#include <llarp/vpn/i_packet_io.hpp>

namespace llarp::win32
{
  struct WinDivertDLL;

  class WinDivert_API
  {
    std::shared_ptr<WinDivertDLL> m_Impl;

   public:
    WinDivert_API();

    /// format an ipv4 in host order to string such that a windivert filter spec can understand it
    std::string
    format_ip(uint32_t ip) const;

    /// create a packet intercepter that uses windivert.
    /// filter_spec describes the kind of traffic we wish to intercept.
    /// pass in a callable that wakes up the main event loop.
    /// we hide all implementation details from other compilation units to prevent issues with
    /// linkage that may arrise.
    std::shared_ptr<llarp::vpn::I_Packet_IO>
    make_intercepter(std::string filter_spec, std::function<void(void)> wakeup) const;
  };
}  // namespace llarp::win32
