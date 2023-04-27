#include "platform_stats.hpp"
#include "addr_mapper.hpp"
#include <llarp/layers/flow/flow_auth.hpp>
#include <llarp/layers/flow/flow_layer.hpp>
#include <llarp/layers/flow/flow_state.hpp>

namespace llarp::layers::platform
{

  /// return true if we want to use an exit.
  bool
  PlatformStats::wants_client_exit() const
  {
    for (const auto& addr : addrs)
    {
      if (addr.allows_transit())
        return true;
    }
    return false;
  }

  /// return true if we want an exit and have one ready.
  bool
  PlatformStats::client_exit_ready() const
  {
    for (const auto& addr : addrs)
    {
      if (addr.allows_transit() and addr.is_ready())
        return true;
    }
    return false;
  }

  MappingStats::MappingStats(const AddressMapping& ent, const flow::FlowLayer& flow_layer)
      : MappingStats{}
  {
    reset();

    if (ent.is_exit())
      set(_mask_routable_traffic);

    auto maybe_state = flow_layer.current_state_for(ent.flow_info);
    if (not maybe_state)
      return;
    const auto& state = *maybe_state;
    // todo: initiator or recip?

    if (state.established())
      set(_mask_established);

    if (state.requires_authentication())
      set(_mask_auth_required);
    switch (state.auth_phase())
    {
      case flow::FlowAuthPhase::auth_more:
      case flow::FlowAuthPhase::auth_req_sent:
        set(_mask_auth_pending);
        return;
      case flow::FlowAuthPhase::auth_ok:
        set(_mask_auth_ok);
        break;
      default:
        break;
    }
  }

}  // namespace llarp::layers::platform
