#ifndef LLARP_ROUTING_HANDLER_HPP
#define LLARP_ROUTING_HANDLER_HPP

#include <llarp/buffer.h>

namespace llarp
{
  namespace routing
  {
    // handles messages on owned paths
    struct IMessageHandler
    {
      virtual bool
      HandleHiddenServiceData(llarp_buffer_t buf) = 0;
    };
  }  // namespace routing
}  // namespace llarp

#endif