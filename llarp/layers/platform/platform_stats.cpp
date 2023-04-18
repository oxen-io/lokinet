#include "platform_stats.hpp"
#include "addr_mapper.hpp"
#include "llarp/layers/flow/flow_layer.hpp"
#include "platform_layer.hpp"

namespace llarp::layers::platform
{

  /// return true if we want to use an exit.
  bool
  PlatformStats::wants_client_exit() const
  {
    for (const auto& addr : addrs)
    {
      if (addr.is_exit())
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
      if (addr.is_exit() and addr.is_ready())
        return true;
    }
    return false;
  }

  namespace stats
  {
    AddrInfo::AddrInfo(const AddressMapping& ent, const flow::FlowLayer& flow)
        : addr{ent}, _flags{0}
    {
      if (ent.is_exit())
        _flags |= _mask_exit;

      if (ent.flow_info and flow.has_flow(*ent.flow_info))
        _flags |= _mask_ready;
    }
  }  // namespace stats

}  // namespace llarp::layers::platform
