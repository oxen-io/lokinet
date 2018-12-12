#ifndef LLARP_ROUTING_ENDPOINT_HPP
#define LLARP_ROUTING_ENDPOINT_HPP

#include <aligned.hpp>
#include <buffer.h>

namespace llarp
{
  using RoutingEndpoint_t = AlignedBuffer< 32 >;

  /// Interface for end to end crypto between endpoints
  struct IRoutingEndpoint
  {
    virtual ~IRoutingEndpoint(){};
  };
}  // namespace llarp

#endif
