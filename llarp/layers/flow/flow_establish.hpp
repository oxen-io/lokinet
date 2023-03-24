#pragma once

#include <functional>
#include <string>
#include <optional>

#include "flow_info.hpp"
#include "flow_auth.hpp"

namespace llarp::layers::flow
{

  /// handles informing an observer at each step of obtaining a flow on the flow layer.
  class FlowEstablish
  {
    std::function<void(std::optional<FlowInfo>, std::string)> _completion_handler;
    std::function<void(FlowAuthPhase, std::string)> _phase_handler;
    std::optional<FlowInfo> _result;

   public:
    FlowEstablish(
        std::function<void(std::optional<FlowInfo>, std::string)> completion_handler,
        std::function<void(FlowAuthPhase, std::string)> phase_handler);

    /// an optional static auth code
    std::string authcode;

    /// how long we wait for establishment timeout
    std::chrono::milliseconds timeout{5s};

    /// enter phase of flow establishment.
    void
    enter_phase(FlowAuthPhase phase, std::string info);

    /// explicitly fail establishment with an error message.
    void
    fail(std::string msg);

    /// explicitly inform that we are done with the establishment and we are ready to send.
    /// calls _completion_handler.
    void
    ready(FlowInfo flow_info);
  };
}  // namespace llarp::layers::flow
