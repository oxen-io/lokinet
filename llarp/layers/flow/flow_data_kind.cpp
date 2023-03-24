#include "flow_data_kind.hpp"

namespace llarp::layers::flow
{

  std::string_view
  ToString(FlowDataKind kind)
  {
    switch (kind)
    {
      case FlowDataKind::auth:
        return "auth";
      case FlowDataKind::direct_ip_unicast:
        return "direct_ip_unicast";
      case FlowDataKind::exit_ip_unicast:
        return "exit_ip_unicast";
      case FlowDataKind::stream_unicast:
        return "stream_unicast";
      default:
        return "unknown";
    }
  }

}  // namespace llarp::layers::flow
