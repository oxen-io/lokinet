#ifndef LLARP_ROUTING_ENDPOINT_HPP
#define LLARP_ROUTING_ENDPOINT_HPP

#include <llarp/aligned.hpp>
#include <llarp/buffer.h>


namespace llarp
{

  typedef AlignedBuffer<32> RoutingEndpoint_t;

  /// Interface for end to end crypto between endpoints
  struct IRoutingEndpoint
  {
    virtual ~IRoutingEndpoint() {};

  };
}

#endif