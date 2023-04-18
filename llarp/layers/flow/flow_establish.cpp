#include "flow_establish.hpp"
#include <functional>
#include <optional>
namespace llarp::layers::flow
{
  static auto logcat = log::Cat("flow-layer");

  FlowEstablish::FlowEstablish(
      std::function<void(std::optional<FlowInfo>, std::string)> completiton_handler,
      std::function<void(FlowAuthPhase, std::string)> phase_handler)
      : _completion_handler{std::move(completiton_handler)}
      , _phase_handler{std::move(phase_handler)}
  {}

  void
  FlowEstablish::fail(std::string info)
  {
    _result = std::nullopt;
    enter_phase(FlowAuthPhase::auth_nack, std::move(info));
  }

  void
  FlowEstablish::ready(FlowInfo flow_info)
  {
    _result = flow_info;
    enter_phase(FlowAuthPhase::auth_ok, "");
    _result = std::nullopt;
  }

  void
  FlowEstablish::enter_phase(FlowAuthPhase phase, std::string info)
  {
    bool completed = phase == FlowAuthPhase::auth_ok or phase == FlowAuthPhase::auth_nack;
    if (completed)
    {
      if (_completion_handler)
        _completion_handler(_result, std::move(info));
      _completion_handler = nullptr;
    }

    if (_phase_handler)
      _phase_handler(phase, info);
  }
}  // namespace llarp::layers::flow
