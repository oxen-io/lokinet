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
    [[nodiscard]] static auto
    DefaultRouteRanges()
    {
      return std::vector<IPRange>{
          IPRange::FromIPv4(0, 0, 0, 0, 1),
          IPRange::FromIPv4(128, 0, 0, 0, 1),
          IPRange{huint128_t{0}, netmask_ipv6_bits(2)},
          IPRange{huint128_t{0x4000'0000'0000'0000UL}, netmask_ipv6_bits(2)},
          IPRange{huint128_t{0x8000'0000'0000'0000UL}, netmask_ipv6_bits(2)},
          IPRange{huint128_t{0xc000'0000'0000'0000UL}, netmask_ipv6_bits(2)}};
    }

   public:
    using IPVariant_t = std::variant<huint32_t, huint128_t>;

    IRouteManager() = default;
    IRouteManager(const IRouteManager&) = delete;
    IRouteManager(IRouteManager&&) = delete;
    virtual ~IRouteManager() = default;

    virtual void
    AddRoute(IPVariant_t ip, IPVariant_t gateway, huint16_t udpport) = 0;

    virtual void
    DelRoute(IPVariant_t ip, IPVariant_t gateway, huint16_t udpport) = 0;

    virtual void
    AddDefaultRouteViaInterface(NetworkInterface& vpn)
    {
      for (auto range : DefaultRouteRanges())
        AddRouteViaInterface(vpn, range);
    }

    virtual void
    DelDefaultRouteViaInterface(NetworkInterface& vpn)
    {
      for (auto range : DefaultRouteRanges())
        DelRouteViaInterface(vpn, range);
    }

    virtual void
    AddRouteViaInterface(NetworkInterface& vpn, IPRange range) = 0;

    virtual void
    DelRouteViaInterface(NetworkInterface& vpn, IPRange range) = 0;

    virtual std::vector<IPVariant_t>
    GetGatewaysNotOnInterface(NetworkInterface& vpn) = 0;

    virtual void
    AddBlackhole(){};

    virtual void
    DelBlackhole(){};
  };

  /// a vpn platform
  /// responsible for obtaining vpn interfaces
  class Platform
  {
   protected:
    /// a pointer to the context that owns the platform
    llarp::Context* const m_OwningContext;

   public:
    explicit Platform(llarp::Context* ctx) : m_OwningContext{ctx}
    {}
    Platform(const Platform&) = delete;
    Platform(Platform&&) = delete;
    virtual ~Platform() = default;

    /// get a new network interface fully configured given the interface info
    /// blocks until ready, throws on error
    virtual std::shared_ptr<NetworkInterface>
    ObtainInterface(InterfaceInfo info) = 0;

    /// get owned ip route manager for managing routing table
    virtual IRouteManager&
    RouteManager() = 0;

    /// tear down vpn platform before destruction
    virtual void
    TearDown(){};
  };

  /// create native vpn platform
  std::shared_ptr<Platform>
  MakeNativePlatform(llarp::Context* ctx);

  /// clean up excess junk from vpn platform on uninstall
  void
  CleanUpPlatform();

}  // namespace llarp::vpn
