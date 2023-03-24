#pragma once

#include <memory>
#include "flow_auth.hpp"

namespace llarp::layers::flow
{

  struct FlowState_Pimpl;

  /// internal state of a flow on the flow layer.
  class FlowState_Base
  {
    std::shared_ptr<FlowState_Pimpl> _pimpl;

   public:
    FlowState_Base(const FlowState_Base&) = delete;
    FlowState_Base(FlowState_Base&&) = delete;

    /// returns true if we have paths that are aligned to the right place on the network to send to
    /// the remote.
    bool
    paths_aligned() const;

    /// returns true if we are to do authentication with the remote.
    bool
    requires_authentication() const;

    /// get the phase we are in with auth.

    FlowAuthPhase
    auth_phase() const;

    /// returns true if we have an established session for sending traffic over to the remote.
    /// any authentication has completed and accepted.
    bool
    established() const;
  };

}  // namespace llarp::layers::flow
