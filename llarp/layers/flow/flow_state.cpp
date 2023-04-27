#include "flow_state.hpp"

#include <llarp/service/endpoint.hpp>
#include <llarp/service/outbound_context.hpp>
#include <memory>
#include "llarp/endpoint_base.hpp"
#include "llarp/layers/flow/flow_auth.hpp"
#include "llarp/path/pathbuilder.hpp"
#include "llarp/path/pathset.hpp"
#include "llarp/service/address.hpp"

namespace llarp::layers::flow
{
  struct FlowState_Pimpl
  {
    FlowAddr remote;
    std::shared_ptr<service::Endpoint> endpoint;
  };

  FlowState::FlowState(FlowState_Pimpl* impl)
  {
    _pimpl.reset(impl);
  }

  bool
  FlowStateInfo::established() const
  {
    return false;
  }

  FlowAuthPhase
  FlowStateInfo::auth_phase() const
  {
    return FlowAuthPhase::auth_ok;
  }

  bool
  FlowStateInfo::requires_authentication() const
  {
    return false;
  }

  FlowStateInfo::FlowStateInfo(const FlowState_Pimpl&)
  {}

  FlowStateInfo
  FlowState::current() const
  {
    return FlowStateInfo{*_pimpl};
  }

}  // namespace llarp::layers::flow
