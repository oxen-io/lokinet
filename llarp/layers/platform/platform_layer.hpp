#pragma once

#include "addr_mapper.hpp"
#include "platform_addr.hpp"
#include "platform_stats.hpp"
#include "os_traffic.hpp"

#include <llarp/config/config.hpp>
#include <llarp/net/ip_packet.hpp>
#include <llarp/net/ip_range.hpp>

#include <llarp/util/formattable.hpp>
#include <llarp/util/types.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace llarp
{
  class EventLoop;
}

namespace llarp::dns
{
  class Server;
}

namespace llarp::quic
{
  // TODO: forward declared no idea if this is an existing type or not.
  class TunnelManager;
}  // namespace llarp::quic

namespace llarp::vpn
{
  class NetworkInterface;
  class IRouteManager;
}  // namespace llarp::vpn

namespace llarp::layers::flow
{
  class FlowLayer;
  class FlowAddr;
  struct FlowInfo;
  struct FlowTraffic;

}  // namespace llarp::layers::flow

namespace llarp::layers::platform
{
  class DNSZone;
  class DNSQueryHandler;

  // handles reading and writing os traffic, like ip packets or whatever embedded lokinet wants to
  // do.
  class OSTraffic_IO_Base
  {
    PlatformAddr _our_addr;
    std::vector<OSTraffic> _recv_queue;
    std::atomic<bool> _running;

   protected:
    std::weak_ptr<EventLoop> _loop;
    std::shared_ptr<vpn::NetworkInterface> _netif;
    std::shared_ptr<quic::TunnelManager> _quic;

    virtual void got_ip_packet(net::IPPacket);

   public:
    explicit OSTraffic_IO_Base(const EventLoop_ptr&);

    virtual ~OSTraffic_IO_Base() = default;

    /// get the platform address of the underlying device managed.
    constexpr const PlatformAddr&
    our_platform_addr() const
    {
      return _our_addr;
    }

    /// attach to a vpn interface. sets up io handlers.
    /// idempotently wakes up wakeup_recv when we get packets read.
    void
    attach(
        std::shared_ptr<vpn::NetworkInterface> netif, std::shared_ptr<EventLoopWakeup> wakeup_recv);

    /// attach to a quic handler. idempotently wakes up wakeup_recv when we get any kind of data
    /// from quic_tun.
    void
    attach(
        std::shared_ptr<quic::TunnelManager> quic_tun,
        std::shared_ptr<EventLoopWakeup> wakeup_recv);

    /// read from the os.
    /// this MUST NOT have any internal userland buffering from previous calls to
    /// read_platform_traffic().
    virtual std::vector<OSTraffic>
    read_platform_traffic();

    /// write traffic to the os.
    virtual void
    write_platform_traffic(std::vector<OSTraffic>&& traff);

    /// stop all future io and detach from event loop.
    void
    detach();
  };

  struct platform_io_wakeup_handler;
  struct add_initial_mappings;

  /// responsible for bridging the os and the flow layer.
  class PlatformLayer
  {
   protected:
    friend struct platform_io_wakeup_handler;
    friend struct add_initial_mappings;

    AbstractRouter& _router;
    flow::FlowLayer& _flow_layer;

    /// called to wake up the loop to read os traffic
    std::shared_ptr<EventLoopWakeup> _os_recv;

    NetworkConfig _netconf;

    /// handler of dns queries.
    std::shared_ptr<DNSQueryHandler> _dns_query_handler;

    std::weak_ptr<vpn::NetworkInterface> _netif;

    std::unique_ptr<OSTraffic_IO_Base> _io;

    std::shared_ptr<DNSZone> _local_dns_zone;
    /// called once per event loop tick after platform layer event loop polled for and consumed io.
    void
    on_io();

    /// make platform specific io with quarks.
    virtual std::unique_ptr<OSTraffic_IO_Base>
    make_io() const;

   public:
    /// holds address mapping state.
    AddrMapper addr_mapper;
    /// underlying io.
    const std::unique_ptr<OSTraffic_IO_Base>& io{_io};

    /// wake this up when the event loop reads ip packets.
    const std::shared_ptr<EventLoopWakeup>& wakeup{_os_recv};

    PlatformLayer(const PlatformLayer&) = delete;
    PlatformLayer(PlatformLayer&&) = delete;

    PlatformLayer(
        AbstractRouter& router, flow::FlowLayer& flow_layer, const NetworkConfig& netconf);

    void
    start();

    void
    stop();

    std::shared_ptr<vpn::NetworkInterface>
    vpn_interface() const;

    vpn::IRouteManager*
    route_manager() const;

    /// return the dns zone for our local self.
    DNSZone&
    local_dns_zone();

    /// called each time we get a bunch of os traffic.
    void
    os_traffic(std::vector<OSTraffic>&& pkts);

    /// give the platform layer flow layer traffic to write to the os.
    void
    flow_traffic(std::vector<flow::FlowTraffic>&& traff);

    /// async resolve dns zone given a flow level name.
    void
    async_obtain_dns_zone(
        std::string name, std::function<void(std::optional<DNSZone>)> result_handler);

    void
    map_remote(
        std::string name,
        std::string auth,
        std::vector<IPRange> ranges,
        std::optional<PlatformAddr> src = std::nullopt,
        std::function<void(std::optional<flow::FlowInfo>, std::string)> result_handler = nullptr);

    /// calls map_remote() for exits
    void
    map_exit(
        std::string name,
        std::string auth,
        std::vector<IPRange> ranges,
        std::function<void(bool, std::string)> result_handler);

    /// unmap an exit from a range
    void
    unmap_exit(flow::FlowAddr remote, std::optional<IPRange> range = std::nullopt);

    /// unmap every exit that is mapped to this range.
    void
    unmap_all_exits_on_range(IPRange range);

    PlatformStats
    current_stats() const;
  };
}  // namespace llarp::layers::platform
