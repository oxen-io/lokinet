#ifndef LLARP_CONSTANTS_LIMITS_HPP
#define LLARP_CONSTANTS_LIMITS_HPP
#include <cstddef>
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
  }  // namespace limits
}  // namespace llarp

#endif