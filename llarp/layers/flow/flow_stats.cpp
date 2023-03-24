#include "flow_stats.hpp"

namespace llarp::layers::flow
{

  bool
  FlowStats::has_good_flow_to(const FlowAddr& remote) const
  {
    for (const auto& [flow_info, stats] : all_flows)
    {
      if (flow_info.dst == remote and stats.state == layers::flow::FlowInfoState::good)
        return true;
    }
    return false;
  }

  std::unordered_set<FlowInfo>
  FlowStats::good_flows() const
  {
    // todo: implement me
    return {};
  }

  std::vector<FlowAddr>
  FlowStats::local_addrs() const
  {
    // todo: implement me
    return {};
  }

  std::unordered_map<FlowAddr, std::string>
  FlowStats::auth_map() const
  {
    // todo: implement me
    return {};
  }
}  // namespace llarp::layers::flow
