#ifndef LLARP_CONSTANTS_LIMITS_HPP
#define LLARP_CONSTANTS_LIMITS_HPP
#include <cstddef>
#include <util/buffer.hpp>

namespace llarp
{
  namespace limits
  {
    /// Limits are a struct that contains all hard and soft limit
    /// parameters for a given mode of operation
    struct LimitParameters
    {
      /// minimum routers needed to run
      std::size_t DefaultMinRouters;
      /// hard limit on router sessions (by pubkey)
      std::size_t DefaultMaxRouters;
    };

    /// snode limit parameters
    const extern LimitParameters snode;

    /// client limit parameters
    const extern LimitParameters client;

    /// limits on lns
    struct LNSLimits
    {
      std::size_t MaxNameSize;

      bool
      NameIsValid(const llarp_buffer_t& namebuf) const;
    };

    const extern LNSLimits lns;

  }  // namespace limits
}  // namespace llarp

#endif
