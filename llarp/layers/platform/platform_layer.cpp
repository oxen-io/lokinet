#include "platform_layer.hpp"
#include "dns_bridge.hpp"
#include "llarp/layers/flow/flow_addr.hpp"
#include "llarp/layers/platform/addr_mapper.hpp"
#include "llarp/layers/platform/platform_addr.hpp"
#include "llarp/net/ip_packet.hpp"
#include "llarp/net/ip_range.hpp"
#include "oxen/log.hpp"

#include <llarp/config/config.hpp>
#include <llarp/ev/ev.hpp>
#include <llarp/layers/flow/flow_layer.hpp>
#include <llarp/dns/server.hpp>
#include <llarp/constants/platform.hpp>
#include <llarp/link/i_link_manager.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/vpn/platform.hpp>
#include <llarp/router/route_poker.hpp>
#include <memory>
#include <nlohmann/detail/string_concat.hpp>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace llarp::layers::platform
{
  static auto logcat = log::Cat("platform-layer");

  void
  OSTraffic_IO_Base::got_ip_packet(net::IPPacket pkt)
  {
    if (not _running.load())
      return;
    auto& os_pkt = _recv_queue.emplace_back();
    os_pkt.kind = EtherType_t::ip_unicast;
    os_pkt.src.ip = net::ipv6addr_t::from_host(pkt.srcv6());
    os_pkt.dst.ip = net::ipv6addr_t::from_host(pkt.dstv6());
    os_pkt.data = pkt.steal();
    log::trace(logcat, "got_ip_packet: {}", os_pkt);
  }

  OSTraffic_IO_Base::OSTraffic_IO_Base(const EventLoop_ptr& loop) : _running{false}, _loop{loop}
  {
    _running.store(true);
  }

  std::vector<OSTraffic>
  OSTraffic_IO_Base::read_platform_traffic()
  {
    std::vector<OSTraffic> ret = std::move(_recv_queue);
    _recv_queue = decltype(_recv_queue){};
    _recv_queue.reserve(ret.capacity());
    log::trace(logcat, "read_platform_traffic: {} packets", ret.size());
    return ret;
  }

  void
  OSTraffic_IO_Base::write_platform_traffic(std::vector<OSTraffic>&& traff)
  {
    if (not _running.load())
      return;
    if (not _netif)
      return;
    log::trace(logcat, "write_platform_traffic: {} packets", traff.size());
    for (auto& os_pkt : traff)
    {
      // todo: figure out plainquic.
      if (os_pkt.kind != EtherType_t::ip_unicast)
        continue;

      net::IPPacket pkt{std::move(os_pkt.data)};
      // drop any malformed packets.
      if (pkt.empty())
        continue;

      _netif->WritePacket(std::move(pkt));
    }
  }

  void
  OSTraffic_IO_Base::attach(
      std::shared_ptr<vpn::NetworkInterface> netif, std::shared_ptr<EventLoopWakeup> wakeup_recv)
  {
    log::debug(logcat, "attach OSTraffic_IO_Base to {}", netif->Info().ifname);
    _netif = std::move(netif);
    _our_addr = PlatformAddr{_netif->Info().addrs[0].range.addr};

    // set up event loop reader.
    // this idempotently wakes up those who want ip packets from us.
    auto loop = _loop.lock();
    if (not loop)
      throw std::runtime_error{"event loop gone"};
    loop->add_network_interface(_netif, [this, wakeup = std::move(wakeup_recv)](auto pkt) {
      if (not _running)
        return;
      got_ip_packet(std::move(pkt));
      wakeup->Trigger();
    });
    _netif->Start();
    log::debug(logcat, "network interface '{}' started up", _netif->Info().ifname);
  }

  void
  OSTraffic_IO_Base::detach()
  {
    if (not _running.exchange(false))
      return;
    log::trace(logcat, "detach from event loop");
    if (_netif)
      _netif->Stop();
    _netif = nullptr;
  }

  void
  PlatformLayer::on_io()
  {
    log::trace(logcat, "on_io()");
    // read all packets from last io cycle.
    auto pkts = _io->read_platform_traffic();
    // if we read nothing we are done.
    if (pkts.empty())
      return;
    os_traffic(std::move(pkts));
  }

  static std::vector<byte_t>
  strip_ip_packet(std::vector<byte_t> data, bool strip_src, bool strip_dst)
  {
    net::IPPacket pkt{std::move(data)};
    // malformed?
    if (pkt.empty())
      return std::vector<byte_t>{};

    if (strip_dst and strip_src)
    {
      pkt.ZeroAddresses();
    }
    else if (strip_src)
    {
      pkt.ZeroSourceAddress();
    }

    return pkt.steal();
  }

  static EtherType_t
  to_ether_type(flow::FlowDataKind kind)
  {
    const std::unordered_map<flow::FlowDataKind, EtherType_t> kind_to_ethertype = {
        {flow::FlowDataKind::auth, EtherType_t::proto_auth},
        {flow::FlowDataKind::stream_unicast, EtherType_t::plainquic},
        {flow::FlowDataKind::direct_ip_unicast, EtherType_t::ip_unicast},
        {flow::FlowDataKind::exit_ip_unicast, EtherType_t::ip_unicast},
    };

    return kind_to_ethertype.at(kind);
  }

  void
  PlatformLayer::flow_traffic(std::vector<flow::FlowTraffic>&& traff)
  {
    std::vector<OSTraffic> os_pkts;
    // for each flow layer traffic ...
    for (auto& pkt : traff)
    {
      // figure out what it is tracked on.
      auto maybe_addr = addr_mapper.get_addr_for_flow(pkt.flow_info);
      if (not maybe_addr)
      {
        // this is traffic for something we are not tracking.
        log::warning(
            logcat,
            "unexpected flow traffic on {}: {} bytes of {}",
            pkt.flow_info,
            pkt.datum.size(),
            pkt.kind);
        continue;
      }
      // translate flow traffic to platform traffic.
      log::trace(logcat, "got {} bytes of {} on {}", pkt.datum.size(), pkt.kind, *maybe_addr);
      os_pkts.emplace_back(OSTraffic{
          to_ether_type(pkt.kind), maybe_addr->dst, maybe_addr->src, std::move(pkt.datum)});
    }
    // write platform traffic.
    if (not os_pkts.empty())
      _io->write_platform_traffic(std::move(os_pkts));
  }

  void
  PlatformLayer::os_traffic(std::vector<OSTraffic>&& traff)
  {
    std::vector<OSTraffic> icmp_reject;
    // for each ip packet...
    for (auto& os_pkt : traff)
    {
      if (os_pkt.kind != EtherType_t::ip_unicast)
        continue;
      if (os_pkt.data.empty())
      {
        // there is nothing here.. ?
        log::warning(logcat, "got empty packet? src={} dst={}", os_pkt.src, os_pkt.dst);
        continue;
      }
      // find an existing flow.
      auto maybe_mapping = addr_mapper.mapping_for(os_pkt.src, os_pkt.dst);
      if ((not maybe_mapping) or (not maybe_mapping->flow_info))
      {
        // we have no flows to that place so we will send an icmp reject back from our address to
        // whoever sent it.
        log::trace(logcat, "no mapping for src={} dst={}", os_pkt.src, os_pkt.dst);
        // dest host unreachable
        auto icmp = net::IPPacket::make_icmp(
            std::move(os_pkt.data), _io->our_platform_addr().as_ipaddr(), 3, 1);

        icmp_reject.emplace_back(
            os_pkt.kind, _io->our_platform_addr(), os_pkt.src, std::move(icmp));
        continue;
      }
      const auto& flow_info = *maybe_mapping->flow_info;

      if (os_pkt.data.size() > flow_info.mtu)
      {
        log::trace(logcat, "packet too big: {} > {}", os_pkt.data.size(), flow_info.mtu);
        // traffic too big. write icmp reject for fragmentation.
        OSTraffic reply;
        reply.kind = os_pkt.kind;
        reply.src = _io->our_platform_addr();
        reply.dst = os_pkt.src;
        reply.data = net::IPPacket::make_icmp(
            std::move(os_pkt.data), reply.src.as_ipaddr(), 3, 4);  // ip fragmentation.
        icmp_reject.push_back(std::move(reply));
      }

      flow::FlowDataKind data_kind = flow::FlowDataKind::direct_ip_unicast;
      bool is_exit = not maybe_mapping->owned_ranges.empty();
      if (is_exit)
        data_kind = flow::FlowDataKind::exit_ip_unicast;

      // get a flow layer source to send with, ...
      auto local_source = _flow_layer.flow_to(flow_info.dst);
      // ... ensure it is sanitized and well formed ...
      auto pkt = strip_ip_packet(std::move(os_pkt.data), true, not is_exit);
      if (pkt.empty())
      {
        log::debug(logcat, "did not strip ip packet on {}", local_source->flow_info);
        continue;
      }

      log::trace(logcat, "send {} bytes to {}", pkt.size(), local_source->flow_info);
      // ... and send the data along.
      local_source->send_to_remote(std::move(pkt), data_kind);
    }
    // write any reject packets we need to.
    if (not icmp_reject.empty())
      _io->write_platform_traffic(std::move(icmp_reject));
  }

  /// this calls the io handlers after an io cycle consumes events.
  struct platform_io_wakeup_handler
  {
    PlatformLayer& driver;
    void
    operator()()
    {
      driver.on_io();
    }
  };

  PlatformLayer::PlatformLayer(
      AbstractRouter& router, flow::FlowLayer& flow_layer, const NetworkConfig& netconf)
      : _router{router}
      , _flow_layer{flow_layer}
      , _os_recv{router.loop()->make_waker(platform_io_wakeup_handler{*this})}
      , _netconf{netconf}
      , _dns_query_handler{std::make_shared<DNSQueryHandler>(*this)}
      , addr_mapper{_netconf.ifaddr(router.Net())}
  {}

  void
  PlatformLayer::start()
  {
    _io = make_io();

    // attach dns resolver.
    if (const auto& dns = _router.get_dns())
      dns->AddResolver(std::weak_ptr<dns::Resolver_Base>{_dns_query_handler});

    auto* vpn = _router.GetVPNPlatform();
    if (not vpn)
      return;

    // create vpn interface and attach to platform layer.
    const auto& _net = _router.Net();
    auto netif = vpn->CreateInterface(vpn::InterfaceInfo{_netconf, _net}, &_router);

    if (not netif)
      throw std::runtime_error{"did not make vpn interface"};
    _netif = netif;
    _io->attach(netif, _os_recv);

    // attach vpn interface to dns and start it.
    if (const auto& dns = _router.get_dns())
      dns->Start(netif->Info().index);
  }

  void
  PlatformLayer::stop()
  {
    log::info(logcat, "stop");
    // stop all io operations on our end and detach from event loop.
    _io->detach();
  }

  /// handles when we have established a flow with a remote.
  struct got_flow_handler
  {
    AddrMapper& mapper;
    AddressMapping entry;
    std::string name;
    std::function<void(std::optional<flow::FlowInfo>, std::string)> result_handler;
    void
    fail(std::string error_reason) const
    {
      result_handler(std::nullopt, "could not obtain flow to '{}': {}"_format(name, error_reason));
    }

    void
    success(const flow::FlowInfo& flow_info)
    {
      entry.flow_info = flow_info;
      mapper.put(std::move(entry));

      result_handler(flow_info, "");
    }

    void
    operator()(std::optional<flow::FlowInfo> maybe_flow_info, std::string error)
    {
      if (maybe_flow_info)
        success(*maybe_flow_info);
      else
        fail(std::move(error));
    }
  };

  /// called to handle when name resolution completes with either failure or success.
  struct resolved_name_handler
  {
    flow::FlowLayer& flow_layer;
    std::string name;
    flow::FlowEstablish flow_establisher;
    bool hydrate_cache{false};

    void
    operator()(std::optional<flow::FlowAddr> maybe_addr)
    {
      if (not maybe_addr)
      {
        flow_establisher.fail("failed to resolve name");
        return;
      }
      // get a flow tag for this remote.
      flow_layer.flow_to(*maybe_addr)->async_ensure_flow(std::move(flow_establisher));
    }
  };

  void
  PlatformLayer::map_remote(
      std::string name,
      std::string auth,
      std::vector<IPRange> dst_ranges,
      std::optional<PlatformAddr> src,
      std::function<void(std::optional<flow::FlowInfo>, std::string)> result_handler)
  {
    if (addr_mapper.is_full())
    {
      result_handler(std::nullopt, "address map full");
      return;
    }
    // reserve a mapping.
    AddressMapping mapping = addr_mapper.allocate_mapping(src.value_or(_io->our_platform_addr()));
    mapping.owned_ranges = std::move(dst_ranges);

    // call this when we got a flow to the guy
    // it will wire up the mapping.
    got_flow_handler got_flow{addr_mapper, std::move(mapping), name, std::move(result_handler)};

    // do any extra handshaking with a flow handshaker, then call the got flow handler
    // todo: this really belongs in flow layer.
    flow::FlowEstablish flow_establisher{std::move(got_flow), nullptr};
    flow_establisher.authcode = std::move(auth);

    resolved_name_handler resolved_name{_flow_layer, name, std::move(flow_establisher)};

    // fire off the lookup.
    _flow_layer.name_resolver.resolve_flow_addr_async(std::move(name), std::move(resolved_name));
  }

  std::shared_ptr<vpn::NetworkInterface>
  PlatformLayer::vpn_interface() const
  {
    return _netif.lock();
  }

  vpn::IRouteManager*
  PlatformLayer::route_manager() const
  {
    if (auto* vpn_plat = _router.GetVPNPlatform())
      return &vpn_plat->RouteManager();
    return nullptr;
  }

  std::unique_ptr<OSTraffic_IO_Base>
  PlatformLayer::make_io() const
  {
    return std::make_unique<OSTraffic_IO_Base>(_router.loop());
  }

  /// handles when we map an exit to an ip range after we have gotten a flow with it.
  struct map_exit_handler
  {
    AbstractRouter& router;
    std::function<void(bool, std::string)> result_handler;
    void
    operator()(std::optional<flow::FlowInfo> found, std::string msg) const
    {
      if (found)
      {
        // found the result, put firewall up.
        if (const auto& poker = router.routePoker())
          poker->Up();
        result_handler(true, std::move(msg));
        return;
      }

      result_handler(false, std::move(msg));
    }
  };

  void
  PlatformLayer::map_exit(
      std::string name,
      std::string auth,
      std::vector<IPRange> ranges,
      std::function<void(bool, std::string)> result_handler)
  {
    map_remote(
        std::move(name),
        std::move(auth),
        std::move(ranges),
        _io->our_platform_addr(),
        map_exit_handler{_router, std::move(result_handler)});
  }

  void
  PlatformLayer::unmap_all_exits_on_range(IPRange range)
  {
    addr_mapper.remove_if_mapping(
        [range](const auto& mapping) -> bool { return mapping.owns_range(range); });
  }

  void
  PlatformLayer::unmap_exit(flow::FlowAddr addr, std::optional<IPRange> range)
  {
    addr_mapper.remove_if_mapping([addr, range](const auto& mapping) -> bool {
      return mapping.flow_info and mapping.flow_info->dst == addr
          and (range ? mapping.owns_range(*range) : true);
    });
  }

  PlatformStats
  PlatformLayer::current_stats() const
  {
    return PlatformStats{};
  }

  DNSZone&
  PlatformLayer::local_dns_zone()
  {
    if (not _local_dns_zone)
      _local_dns_zone = std::make_shared<DNSZone>(addr_mapper, _flow_layer.local_addr());
    return *_local_dns_zone;
  }

}  // namespace llarp::layers::platform
