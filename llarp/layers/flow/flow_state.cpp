#include "flow_state.hpp"

#include <llarp/service/outbound_context.hpp>
#include <memory>

namespace llarp::layers::flow
{
  struct FlowState_Pimpl
  {
    std::shared_ptr<service::OutboundContext> obctx;
  };
}  // namespace llarp::layers::flow
