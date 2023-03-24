#pragma once

#include "flow_data_kind.hpp"
#include "flow_info.hpp"

namespace llarp::layers::flow
{

  struct FlowTraffic
  {
    FlowInfo flow_info;
    std::vector<byte_t> datum;
    FlowDataKind kind;
  };

}  // namespace llarp::layers::flow
