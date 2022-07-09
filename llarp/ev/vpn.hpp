#pragma once

#include <llarp/net/ip_range.hpp>
#include <llarp/net/ip_packet.hpp>
#include <set>

#include <oxenc/variant.h>

namespace llarp
{
  struct Context;
  struct AbstractRouter;
}  // namespace llarp

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

    /// idempotently wake up the upper layers as needed (platform dependant)
    virtual void
    MaybeWakeUpperLayers() const {};
  };

  class IRouteManager
  {
   public:
    using IPVariant_t = std::variant<huint32_t, huint128_t>;

    IRouteManager() = default;
    IRouteManager(const IRouteManager&) = delete;
    IRouteManager(IRouteManager&&) = delete;
    virtual ~IRouteManager() = default;

    virtual const llarp::net::Platform*
    Net_ptr() const;

    inline const llarp::net::Platform&
    Net() const
    {
      return *Net_ptr();
    }

    virtual void
    AddRoute(IPVariant_t ip, IPVariant_t gateway) = 0;

    virtual void
    DelRoute(IPVariant_t ip, IPVariant_t gateway) = 0;

    virtual void
    AddDefaultRouteViaInterface(std::string ifname) = 0;

    virtual void
    DelDefaultRouteViaInterface(std::string ifname) = 0;

    virtual void
    AddRouteViaInterface(NetworkInterface& vpn, IPRange range) = 0;

    virtual void
    DelRouteViaInterface(NetworkInterface& vpn, IPRange range) = 0;

    virtual std::vector<IPVariant_t>
    GetGatewaysNotOnInterface(std::string ifname) = 0;

    virtual void
    AddBlackhole(){};

    virtual void
    DelBlackhole(){};
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
    ObtainInterface(InterfaceInfo info, AbstractRouter* router) = 0;

    /// get owned ip route manager for managing routing table
    virtual IRouteManager&
    RouteManager() = 0;
  };

  /// create native vpn platform
  std::shared_ptr<Platform>
  MakeNativePlatform(llarp::Context* ctx);

}  // namespace llarp::vpn
