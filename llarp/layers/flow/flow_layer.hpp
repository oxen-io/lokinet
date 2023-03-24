#pragma once

#include "flow_addr.hpp"
#include "flow_identity.hpp"
#include "flow_info.hpp"
#include "flow_tag.hpp"
#include "flow_stats.hpp"
#include "flow_traffic.hpp"
#include "name_cache.hpp"
#include "name_resolver.hpp"

#include <llarp/config/config.hpp>
#include <memory>
#include <vector>

namespace llarp
{
  struct AbstractRouter;
}

namespace llarp::service
{
  struct Endpoint;
}

namespace llarp::layers::flow
{

  /// flow layer's context that holds all the things in the flow layer
  class FlowLayer
  {
    const NetworkConfig _conf;
    std::vector<std::shared_ptr<FlowIdentity>> _local_flows;

    FlowIdentityPrivateKeys _privkeys;

    NameCache _name_cache;

    std::shared_ptr<service::Endpoint> _deprecated_endpoint;

    /// flow layer traffic we got from the void.
    std::vector<FlowTraffic> _recv;

    /// wake this up to send things out the lower layers.
    std::shared_ptr<EventLoopWakeup> _wakeup_send;
    /// wakes up the upper layers to tell it we recieved flow layer traffic.
    std::shared_ptr<EventLoopWakeup> _wakeup_recv;

    /// ensure privkeys are generated and persisted if requested by configuration.
    /// throws on any kind of failure. TODO: document what is thrown when.
    void
    maybe_store_or_load_privkeys();

    /// generate a flow tag we dont current have tracked.
    FlowTag
    unique_flow_tag() const;

    /// return true if we have a flow tag tracked.
    bool
    has_flow_tag(const FlowTag&) const;

   public:
    FlowLayer(AbstractRouter&, NetworkConfig);
    FlowLayer(const FlowLayer&) = delete;
    FlowLayer(FlowLayer&&) = delete;

    /// get flow layer's informational stats at this moment in time.
    FlowStats
    current_stats() const;

    /// return true if we have this flow.
    bool
    has_flow(const flow::FlowInfo& flow_info) const;

    /// remove a flow we have tracked. does nothing if we do not have it tracked.
    void
    remove_flow(const flow::FlowInfo& flow_info);

    /// get our flow address of our local given a flow tag.
    /// if we give a nullopt flow tag we get our "default" inbound flow address we publish to the
    /// network.
    const FlowAddr&
    local_addr(const std::optional<FlowTag>& maybe_tag = std::nullopt) const;

    /// get our DEPRECATED endpointbase for our local "us"
    std::shared_ptr<service::Endpoint>
    local_deprecated_loki_endpoint() const;

    /// autovivify a flow to a remote.
    std::shared_ptr<FlowIdentity>
    flow_to(const FlowAddr& to);

    /// pop off all flow layer traffic that we have processed.
    std::vector<FlowTraffic>
    poll_flow_traffic();

    /// synthetically inject flow layer traffic into the flow layer
    void
    offer_flow_traffic(FlowTraffic&& traff);

    void
    start();

    void
    stop();

    NameResolver name_resolver;

    AbstractRouter& router;
    /// wake this up when you send stuff on the flow layer.
    const std::shared_ptr<EventLoopWakeup>& wakeup_send{_wakeup_send};
  };
}  // namespace llarp::layers::flow
