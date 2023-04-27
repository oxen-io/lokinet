#pragma once

#include <bits/types/clock_t.h>
#include <charconv>
#include <cstdint>
#include <unordered_set>

#include <llarp/layers/flow/flow_addr.hpp>
#include <llarp/net/ip_range_map.hpp>

#include "addr_mapper.hpp"
#include "llarp/layers/flow/flow_info.hpp"
#include "platform_addr.hpp"

namespace llarp::layers::flow
{
  class FlowLayer;
}

namespace llarp::layers::platform
{

  /// stats on a specific addrmapper mapping.
  struct MappingStats
  {
    using clock_t = std::chrono::steady_clock;
    /// source and destination os addresses.
    PlatformAddr src, dst;

    /// local and remote flow layer addresses for this mapping.
    flow::FlowAddr laddr, raddr;

    /// when this mapping was created.
    clock_t::time_point created_at;

    /// when this mapping first established a flow to the remove.
    std::optional<clock_t::time_point> established_at;

    /// when this mapping was last active at.
    std::optional<clock_t::time_point> last_active_at;

    MappingStats() = default;
    MappingStats(const AddressMapping& ent, const flow::FlowLayer& flow_layer);

    /// return true if this flow allows us to send routable ip traffic (exit traffic) over it.
    constexpr bool
    allows_transit() const
    {
      return test(_mask_routable_traffic);
    }

    /// return true if this flow is up and running.
    constexpr bool
    is_ready() const
    {
      return traffic_permitted() and test(_mask_established);
    }

    /// return true if we have been permitted by the remote to transfer traffic.
    constexpr bool
    traffic_permitted() const
    {
      if (requires_authentication())
        return has_authenticated();
      return true;
    }

    /// return true if we have completed an authentication with the remote.
    constexpr bool
    has_authenticated() const
    {
      return test(_mask_auth_ok);
    }

    /// return true if the flow requires authentication to transfer traffic.
    constexpr bool
    requires_authentication() const
    {
      return test(_mask_auth_required);
    }

    /// returns true if we were the end that initiated this mapping.
    constexpr bool
    locally_initiated() const
    {
      return test(_mask_initiated);
    }

   private:
    /// the underlying flow is etablished.
    static inline constexpr auto _mask_established{1 << 0};
    /// we are required to do auth with this remote.
    static inline constexpr auto _mask_auth_required{1 << 1};
    /// we are currently negotiating auth.
    static inline constexpr auto _mask_auth_pending{1 << 2};
    /// we have successfully negotiated auth.
    static inline constexpr auto _mask_auth_ok{1 << 3};
    /// we are the one who initiated this flow.
    static inline constexpr auto _mask_initiated{1 << 4};
    /// routable transit traffic is permitted on this flow.
    static inline constexpr auto _mask_routable_traffic{1 << 5};

    /// the traffic on this flow is throttled by the network.
    static inline constexpr auto _mask_bw_limited{1 << 6};

    /// initial state flags start off in.
    static inline constexpr auto _initial_flags = _mask_initiated | _mask_bw_limited;

    /// test flag mask set
    constexpr bool
    test(int mask) const
    {
      return _flags & mask;
    }

    /// set flag mask
    constexpr void
    set(int mask)
    {
      _flags |= mask;
    }

    /// reset flags to default values;
    constexpr void
    reset()
    {
      _flags = _initial_flags;
    }

    int _flags;
  };

  struct PlatformStats
  {
    /// flow addr / platform addr mappings that we have allocated.
    std::vector<MappingStats> addrs;

    /// return true if we want to use an exit.
    bool
    wants_client_exit() const;

    /// return true if we want an exit and have one ready.
    bool
    client_exit_ready() const;
  };
}  // namespace llarp::layers::platform
