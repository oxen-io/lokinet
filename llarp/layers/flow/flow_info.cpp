#include "flow_info.hpp"

namespace llarp::layers::flow
{
  bool
  FlowInfo::operator==(const FlowInfo& other) const
  {
    return src == other.src and dst == other.dst and tag == other.tag;
  }

  std::string
  FlowInfo::ToString() const
  {
    return fmt::format("[flow src={} dst={} tag={} mtu={}]", src, dst, tag, int{mtu});
  }
}  // namespace llarp::layers::flow
