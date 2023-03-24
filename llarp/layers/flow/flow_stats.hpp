#pragma once

#include <functional>
#include <unordered_set>
#include "flow_addr.hpp"
#include "flow_info.hpp"

namespace llarp::layers::flow
{

  enum class FlowInfoState
  {
    good,
    active,
    idle,
    stalled
  };

  /// informational data about a flow we have
  struct FlowInfoStats
  {
    FlowInfoState state;
    /// put extra info here
  };

  struct FlowStats
  {
    std::unordered_map<FlowInfo, FlowInfoStats> all_flows;

    /// default inbound flow address.  (our .snode / .loki address)
    FlowAddr local_addr;

    /// get all local addresses with the first element being local_addr.
    std::vector<FlowAddr>
    local_addrs() const;

    /// get all flow infos that are considered "good".
    std::unordered_set<FlowInfo>
    good_flows() const;

    /// return true if we have a flow to the remote that is "good".
    bool
    has_good_flow_to(const FlowAddr& remote) const;

    /// all flow addrs with auth codes.
    std::unordered_map<FlowAddr, std::string>
    auth_map() const;
  };

}  // namespace llarp::layers::flow
