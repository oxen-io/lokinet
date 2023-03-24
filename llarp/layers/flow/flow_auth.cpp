#include "flow_auth.hpp"

namespace llarp::layers::flow
{
  std::string_view
  ToString(FlowAuthPhase phase)
  {
    switch (phase)
    {
      case FlowAuthPhase::auth_more:
        return "auth_more";
      case FlowAuthPhase::auth_nack:
        return "auth_nack";
      case FlowAuthPhase::auth_req_not_sent:
        return "auth_req_not_sent";
      case FlowAuthPhase::auth_req_sent:
        return "auth_req_sent";
      case FlowAuthPhase::auth_ok:
        return "auth_ok";
      default:
        return "unknown";
    }
  }

}  // namespace llarp::layers::flow
