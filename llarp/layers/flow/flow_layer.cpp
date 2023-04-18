#include "flow_layer.hpp"
#include <oxenc/bt_producer.h>
#include <oxenc/bt_serialize.h>
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include <llarp/router/abstractrouter.hpp>
#include <llarp/service/endpoint.hpp>
#include <llarp/path/path_context.hpp>
#include <vector>

namespace llarp::layers::flow
{

  static auto logcat = log::Cat("flow-layer");

  void
  FlowLayer::maybe_store_or_load_privkeys()
  {
    if (auto maybe_keyfile = _conf.m_keyfile)
    {
      // put down existing private keys if it's not on disk
      const auto& keyfile = *maybe_keyfile;
      if (not fs::exists(keyfile))
      {
        if (auto err = util::EnsurePrivateFile(keyfile))
          throw std::runtime_error{"failed to create file '{}': {}"_format(keyfile, err.message())};
        std::string_view keys{
            reinterpret_cast<const char*>(_privkeys.identity.data()), _privkeys.identity.size()};
        util::dump_file(keyfile, oxenc::bt_serialize(keys));
        return;
      }
      // otherwise we load the identity keys from disk
      // todo: migrate old format?
      auto data = oxenc::bt_deserialize<std::string>(util::slurp_file(keyfile));
      if (data.size() != _privkeys.identity.size())
        throw std::runtime_error{
            "privkey has bad size: {} != {}"_format(data.size(), _privkeys.identity.size())};

      std::copy(data.begin(), data.end(), _privkeys.identity.begin());
    }
  }

  /// handler of feeding flow traffic from flow layer to platform layer.
  struct platform_layer_waker
  {
    FlowLayer& flow_layer;

    void
    operator()()
    {
      // propagate flow traffic up to platform layer.
      if (auto traffic = flow_layer.poll_flow_traffic(); not traffic.empty())
        flow_layer.router.get_layers()->platform->flow_traffic(std::move(traffic));
      // continue feeding any downstream traffic into the flow layer.
      flow_layer.router.pathContext().PumpDownstream();
    }
  };

  /// handler of sending flow layer stuff out to the void.
  /// called after we queued sending all the things in the flow layer in the current event loop
  /// cycle.
  struct flow_layer_waker
  {
    FlowLayer& flow_layer;
    void
    operator()()
    {
      flow_layer.local_deprecated_loki_endpoint()->Pump(time_now_ms());
      // take the things we need to send out on paths and send them out.
      flow_layer.router.pathContext().PumpUpstream();
    }
  };

  FlowLayer::FlowLayer(AbstractRouter& r, NetworkConfig conf)
      : _conf{std::move(conf)}
      , _privkeys{FlowIdentityPrivateKeys::keygen()}
      , _name_cache{}
      , _deprecated_endpoint{std::make_shared<service::Endpoint>(r)}
      , _wakeup_send{r.loop()->make_waker(flow_layer_waker{*this})}
      , _wakeup_recv{r.loop()->make_waker(platform_layer_waker{*this})}
      , name_resolver{_name_cache, *this}
      , router{r}
  {}

  const FlowAddr&
  FlowLayer::local_addr(const std::optional<FlowTag>& maybe_tag) const
  {
    if (maybe_tag)
    {
      const auto& flow_tag = *maybe_tag;
      for (const auto& local_flow : _local_flows)
      {
        if (local_flow->flow_info.tag == flow_tag)
          return local_flow->flow_info.src;
      }
      throw std::invalid_argument{"no such local address for flow tag: {}"_format(flow_tag)};
    }
    return _privkeys.public_addr();
  }

  void
  FlowLayer::start()
  {
    log::info(logcat, "flow layer starting");
    const auto& _dns_conf = router.GetConfig()->dns;
    if (not _deprecated_endpoint->Configure(_conf, _dns_conf))
      throw std::runtime_error{"deprecated endpoint failed to configure"};

    if (not _deprecated_endpoint->Start())
      throw std::runtime_error{"deprecated endpoint did not start"};

    router.loop()->call_every(
        100ms, _deprecated_endpoint, [ep = _deprecated_endpoint]() { ep->Tick(ep->Now()); });
    log::info(logcat, "flow layer up");
  }

  void
  FlowLayer::tick()
  {
    if (_deprecated_endpoint)
    {
      log::trace(logcat, "tick");
      _deprecated_endpoint->Tick(router.Now());
    }
    log::trace(logcat, "ticked");
  }

  void
  FlowLayer::stop()
  {
    log::info(logcat, "flow layer stopping");
    if (_deprecated_endpoint)
    {
      if (not _deprecated_endpoint->Stop())
        throw std::runtime_error{"deprecated endpoint did not stop"};
    }
  }

  void
  FlowLayer::offer_flow_traffic(FlowTraffic&& traff)
  {
    _recv.push_back(traff);
    _wakeup_recv->Trigger();
  }

  bool
  FlowLayer::has_flow(const FlowInfo& flow_info) const
  {
    for (const auto& _flow : _local_flows)
    {
      if (_flow->flow_info == flow_info)
        return true;
    }
    return false;
  }

  void
  FlowLayer::remove_flow(const FlowInfo& flow_info)
  {
    for (auto itr = _local_flows.begin(); itr != _local_flows.end();)
    {
      if ((*itr)->flow_info == flow_info)
        itr = _local_flows.erase(itr);
      else
        ++itr;
    }
  }

  std::vector<FlowTraffic>
  FlowLayer::poll_flow_traffic()
  {
    std::vector<FlowTraffic> ret{_recv.capacity()};
    std::swap(ret, _recv);
    return ret;
  }

  FlowStats
  FlowLayer::current_stats() const
  {
    // todo: implement
    return FlowStats{};
  }

  FlowTag
  FlowLayer::unique_flow_tag() const
  {
    FlowTag flow_tag{};
    do
    {
      flow_tag.Randomize();
    } while (has_flow_tag(flow_tag));
    return flow_tag;
  }

  bool
  FlowLayer::has_flow_tag(const FlowTag& flow_tag) const
  {
    for (const auto& local_flow : _local_flows)
    {
      if (local_flow->flow_info.tag == flow_tag)
        return true;
    }
    return false;
  }

  std::shared_ptr<service::Endpoint>
  FlowLayer::local_deprecated_loki_endpoint() const
  {
    return _deprecated_endpoint;
  }

  std::shared_ptr<FlowIdentity>
  FlowLayer::flow_to(const FlowAddr& addr)
  {
    for (const auto& local_flow : _local_flows)
    {
      if (local_flow->flow_info.dst == addr)
        return local_flow;
    }
    return _local_flows.emplace_back(std::make_shared<FlowIdentity>(*this, addr, _privkeys));
  }
}  // namespace llarp::layers::flow
