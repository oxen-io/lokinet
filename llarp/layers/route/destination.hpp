#pragma once

#include <llarp/crypto/types.hpp>
#include <optional>
#include <llarp/layers/flow/flow_info.hpp>
#include <llarp/path/transit_hop.hpp>

namespace llarp::layers::route
{
  /// a routable destination when we are an endpoint router transiting routing layer traffic.
  class Destination
  {
   public:
    PubKey src;
    PubKey dst;
    /// the info of the transit hop that we are located.
    path::TransitHopInfo onion_info;

    /// flow layer convo this routing destination is fascilitating. if applicable.
    std::optional<flow::FlowInfo> flow_info;
  };
}  // namespace llarp::layers::route
