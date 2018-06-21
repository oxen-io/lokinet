#ifndef LLARP_ENDPOINT_HANDLER_HPP
#define LLARP_ENDPOINT_HANDLER_HPP

#include <llarp/buffer.h>

namespace llarp
{
  // hidden service endpoint handler
  struct IEndpointHandler
  {
    ~IEndpointHandler(){};

    virtual void
    HandleMessage(llarp_buffer_t buf) = 0;
  };
}  // namespace llarp

#endif