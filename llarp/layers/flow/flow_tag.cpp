#include "flow_tag.hpp"

namespace llarp::layers::flow
{

  FlowTag
  FlowTag::random()
  {
    FlowTag tag;
    tag.Randomize();
    return tag;
  }

  std::string
  FlowTag::ToString() const
  {
    return fmt::format("[flowtag '{}']", ToHex());
  }

}  // namespace llarp::layers::flow
