#include "flow_info.hpp"

namespace llarp::layers::flow
{
  bool
  FlowInfo::operator==(const FlowInfo& other) const
  {
    return src == other.src and dst == other.dst;
  }

  std::string
  FlowInfo::ToString() const
  {
    return fmt::format(
        "[flow src={} dst={} tags=({}) mtu={}]",
        src,
        dst,
        fmt::join(flow_tags.begin(), flow_tags.end(), ","),
        int{mtu});
  }

}  // namespace llarp::layers::flow
