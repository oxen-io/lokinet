#include "platform_layer.hpp"
#include <bits/chrono.h>
#include "dns_bridge.hpp"
#include "llarp/layers/flow/flow_info.hpp"
#include "llarp/layers/platform/addr_mapper.hpp"
#include "llarp/layers/platform/ethertype.hpp"
#include "llarp/layers/platform/platform_addr.hpp"
#include "llarp/net/ip_range.hpp"
#include "llarp/service/auth.hpp"
#include "oxen/log.hpp"

#include <chrono>
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
#include <unordered_set>
#include <utility>
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
    _netif = std::move(netif);

    log::debug(logcat, "attach OSTraffic_IO_Base to {}", _netif->Info().ifname);
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

  /// a helper type that resolves a remote after putting an address mapping in for it.
  /// calls result_handlers for resolve result.
  /// can unmap entry on failure if desired.
  struct map_remote_attempt
  {
    std::string name;
    AddrMapper& addr_mapper;
    llarp::layers::flow::FlowLayer& flow_layer;
    AddressMapping& mapping;
    std::string auth;
    std::function<void(std::optional<flow::FlowInfo>, std::string)> result_handler;
    bool unmap_on_fail{false};

    void
    attempt();

    inline void
    operator()()
    {
      attempt();
    }
  };

  /// a functor that will try to set up all address mappings we provided in config.
  struct add_initial_mappings : std::enable_shared_from_this<add_initial_mappings>
  {
    PlatformLayer& plat;

    struct AttemptInfo
    {
      decltype(AddressMapConfig::mappings)::mapped_type value;
      std::chrono::duration<double> backoff{0ms};
      bool busy{false};

      static constexpr std::chrono::duration<double> max_backoff = 5min;

      auto
      apply_backoff()
      {
        backoff += 50ms;
        backoff = std::min(backoff * 1.25, max_backoff);
        return std::chrono::duration_cast<std::chrono::milliseconds>(backoff);
      }
    };

    // name of exit -> attempt info mapping.
    std::unordered_map<std::string, AttemptInfo> pending;

    explicit add_initial_mappings(PlatformLayer& _plat) : plat{_plat}
    {
      for (const auto& [name, info] : plat._netconf.addr_map.mappings)
      {
        // initial attempt backoff of 50ms.
        auto& attempt = pending[name] = AttemptInfo{info};

        // make sure src is set.
        auto& mapping = std::get<0>(attempt.value);
        if (not mapping.src.ip.n)
          mapping.src = plat._io->our_platform_addr();
      }
    }

    /// make an attempt on all pending entries to be mapped.
    void
    operator()()
    {
      if (pending.empty())
      {
        // there is nothing left to do.
        // this will make the shared_ptr no longer exist.
        _self.reset();
        return;
      }

      for (auto& [name, info] : pending)
      {
        if (info.busy)
          continue;

        log::info(logcat, "try looking up {}", name);
        // mark as requesting.
        info.busy = true;
        // do initial attempt.
        if (info.backoff == 0ms)
          plat._router.loop()->call_soon(make_attempt(name));
        else
          plat._router.loop()->call_later(info.apply_backoff(), make_attempt(name));
      }
    }

    /// return ourself after owning a shared pointer to ourself.
    /// this will keep ourself alive.
    add_initial_mappings&
    keep_alive()
    {
      _self = shared_from_this();
      return *this;
    }

   private:
    map_remote_attempt
    make_attempt(std::string name)
    {
      auto& ent = pending[name];
      auto& mapping = std::get<0>(ent.value);
      const auto& auth = std::get<1>(ent.value);

      map_remote_attempt map_attempt{
          name,
          plat.addr_mapper,
          plat._flow_layer,
          mapping,
          auth.token,
          [this, name = name](auto maybe, auto msg) { on_result(maybe, msg, name); }};
    }

    void
    on_result(std::optional<flow::FlowInfo> maybe, std::string msg, std::string name)
    {
      auto itr = pending.find(name);
      if (itr == pending.end())
        return;

      bool ok{maybe};
      if (ok)
      {
        log::info(logcat, "mapped {}: {}", name, msg);
        // put address mapping.
        auto& mapping = std::get<0>(itr->second.value);
        mapping.flow_info = maybe;
        plat.addr_mapper.put(std::move(mapping));
        // prevent any additional attempts.
        pending.erase(itr);
        return;
      }
      itr->second.busy = false;
      log::info(logcat, "failed to map {}: {}", name, msg);
    }

    /// a copy of itself, cleared to destroy itself.
    std::shared_ptr<add_initial_mappings> _self;
  };

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

    auto mapper = std::make_shared<add_initial_mappings>(*this);
    _router.loop()->call_every(100ms, mapper, mapper->keep_alive());
  }

  void
  PlatformLayer::stop()
  {
    log::info(logcat, "stop");
    // stop all io operations on our end and detach from event loop.
    if (_io)
      _io->detach();
  }

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
        std::string msg{"failed to resolve name '{}'"_format(name)};
        log::info(logcat, msg);
        flow_establisher.fail(msg);
        return;
      }
      log::info(logcat, "resolved {} to {}", name, *maybe_addr);
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

    // reserve a mapping. dst is set even if it is an exit.
    auto& mapping = addr_mapper.allocate_mapping(src);
    map_remote_attempt map_remote{name, addr_mapper, _flow_layer, mapping};
    // add exits.
    mapping.owned_ranges = std::move(dst_ranges);

    // do the actual attempt.
    map_remote();
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
  /// this puts the route poker up.
  struct map_exit_handler
  {
    AbstractRouter& router;

    std::function<void(bool, std::string)> result_handler;
    void
    operator()(std::optional<flow::FlowInfo> found, std::string msg) const
    {
      if (found)
      {
        log::info(logcat, "got exit {}: '{}'", found->dst, msg);
        // found the result, put firewall up.
        if (const auto& poker = router.routePoker())
          poker->Up();
        result_handler(true, std::move(msg));
        return;
      }
      log::info(logcat, "exit not made: {}", msg);
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
    log::info(logcat, "map exit {}", name);

    map_remote(
        std::move(name),
        std::move(auth),
        std::move(ranges),
        _io->our_platform_addr(),
        map_exit_handler{_router, std::move(result_handler)});
  }

  void
  map_remote_attempt::attempt()
  {
    // wrap the result handler to unmap the address mapping if that was desired.
    decltype(result_handler) handler = [result_handler = std::move(result_handler), this](
                                           auto maybe, auto msg) {
      if (unmap_on_fail and not maybe)
        addr_mapper.unmap(mapping);

      result_handler(maybe, msg);
    };

    // these events are called in bottom to top order.
    // start from the end of the function and read the comments blocks in reverse order for the
    // sequence of events.

    // try to etablish a flow and call a handler that will do something when we get a flow or failed
    // to get a flow. this does any additional pre setup auth with the remote before handing over
    // the flow to those who care.
    flow::FlowEstablish flow_establisher{std::move(handler), nullptr};
    flow_establisher.authcode = std::move(auth);

    // called when we resolved a name to the long form address.
    // if we failed, we will fail the flow establisher we could not establish a flow because of a
    // name resolve error. if we succeeded we will attempt to establish a flow on this flow
    // establisher.
    resolved_name_handler resolved_name{flow_layer, name, std::move(flow_establisher)};

    // resolve the remote name to a long form address if it is needed.
    flow_layer.name_resolver.resolve_flow_addr_async(std::move(name), std::move(resolved_name));
  }

  void
  PlatformLayer::unmap_all_exits_on_range(IPRange range)
  {
    log::info(logcat, "unmap all on {}", range);
    addr_mapper.remove_if_mapping(
        [range](const auto& mapping) -> bool { return mapping.owns_range(range); });
  }

  void
  PlatformLayer::unmap_exit(flow::FlowAddr addr, std::optional<IPRange> range)
  {
    std::optional<std::string> range_str;
    if (range)
      range_str = range->ToString();

    log::info(logcat, "unmap {} via {}", addr, range_str.value_or("all"));

    addr_mapper.remove_if_mapping([addr, range](const auto& mapping) -> bool {
      return mapping.flow_info and mapping.flow_info->dst == addr
          and (range ? mapping.owns_range(*range) : true);
    });
  }

  PlatformStats
  PlatformLayer::current_stats() const
  {
    PlatformStats st{};
    addr_mapper.view_all_entries([&](const auto& ent) {
      st.addrs.emplace_back(ent, _flow_layer);
      return true;
    });

    return st;
  }

  DNSZone&
  PlatformLayer::local_dns_zone()
  {
    if (not _local_dns_zone)
      _local_dns_zone = std::make_shared<DNSZone>(addr_mapper, _flow_layer.local_addr());
    return *_local_dns_zone;
  }

}  // namespace llarp::layers::platform
