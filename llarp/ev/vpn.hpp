#pragma once

#include <llarp/net/ip_range.hpp>
#include <llarp/net/ip_packet.hpp>
#include <set>

namespace llarp
{
  struct Context;
}

namespace llarp::vpn
{
  struct InterfaceAddress
  {
    constexpr InterfaceAddress(IPRange r, int f = AF_INET) : range{std::move(r)}, fam{f}
    {}
    IPRange range;
    int fam;
    bool
    operator<(const InterfaceAddress& other) const
    {
      return range < other.range or fam < other.fam;
    }
  };

  struct InterfaceInfo
  {
    std::string ifname;
    huint32_t dnsaddr;
    std::set<InterfaceAddress> addrs;
  };

  /// a vpn network interface
  class NetworkInterface
  {
   public:
    NetworkInterface() = default;
    NetworkInterface(const NetworkInterface&) = delete;
    NetworkInterface(NetworkInterface&&) = delete;

    virtual ~NetworkInterface() = default;

    /// get pollable fd for reading
    virtual int
    PollFD() const = 0;

    /// the interface's name
    virtual std::string
    IfName() const = 0;

    /// read next ip packet, return an empty packet if there are none ready.
    virtual net::IPPacket
    ReadNextPacket() = 0;

    /// write a packet to the interface
    /// returns false if we dropped it
    virtual bool
    WritePacket(net::IPPacket pkt) = 0;
  };

  /// a vpn platform
  /// responsible for obtaining vpn interfaces
  class Platform
  {
   public:
    Platform() = default;
    Platform(const Platform&) = delete;
    Platform(Platform&&) = delete;
    virtual ~Platform() = default;

    /// get a new network interface fully configured given the interface info
    /// blocks until ready, throws on error
    virtual std::shared_ptr<NetworkInterface>
    ObtainInterface(InterfaceInfo info) = 0;
  };

  /// create native vpn platform
  std::shared_ptr<Platform>
  MakeNativePlatform(llarp::Context* ctx);

}  // namespace llarp::vpn
