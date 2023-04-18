#pragma once

#include <cstdint>
#include <unordered_set>

#include <llarp/layers/flow/flow_addr.hpp>
#include <llarp/net/ip_range_map.hpp>

#include "addr_mapper.hpp"
#include "platform_addr.hpp"

namespace llarp::layers::flow
{
  class FlowLayer;
}

namespace llarp::layers::platform
{

  namespace stats
  {

    struct AddrInfo
    {
      AddressMapping addr;

      AddrInfo(const AddressMapping& ent, const flow::FlowLayer& flow_layer);

      constexpr bool
      is_exit() const
      {
        return flag(_mask_exit);
      }

      constexpr bool
      is_ready() const
      {
        return flag(_mask_ready) and is_authenticated();
      }

      constexpr bool
      is_authenticated() const
      {
        if (not flag(_mask_auth_required))
          return true;
        return flag(_mask_auth_ok);
      }

     private:
      /// the underlying flow is ready to do io
      static inline constexpr auto _mask_ready{1 << 0};
      /// we are required to do auth with this remote.
      static inline constexpr auto _mask_auth_required{1 << 1};
      static inline constexpr auto _mask_auth_ok{1 << 2};
      static inline constexpr auto _mask_inbound{1 << 3};
      static inline constexpr auto _mask_exit{1 << 4};

      constexpr bool
      flag(int mask) const
      {
        return _flags & mask;
      }

      int _flags{};
    };
  }  // namespace stats

  struct PlatformStats
  {
    /// flow addr / platform addr mappings that we have allocated.
    std::vector<stats::AddrInfo> addrs;

    /// return true if we want to use an exit.
    bool
    wants_client_exit() const;

    /// return true if we want an exit and have one ready.
    bool
    client_exit_ready() const;
  };
}  // namespace llarp::layers::platform
