#ifndef LLARP_ROUTING_HANDLER_HPP
#define LLARP_ROUTING_HANDLER_HPP

#include <llarp/buffer.h>
#include <llarp/dht.hpp>
#include <llarp/messages/path_confirm.hpp>
#include <llarp/messages/path_latency.hpp>

namespace llarp
{
  namespace routing
  {
    // handles messages on owned paths
    struct IMessageHandler
    {
      virtual bool
      HandleHiddenServiceData(llarp_buffer_t buf) = 0;

      virtual bool
      HandlePathConfirmMessage(const PathConfirmMessage* msg) = 0;

      virtual bool
      HandlePathLatencyMessage(const PathLatencyMessage* msg) = 0;

      virtual bool
      HandleDHTMessage(const llarp::dht::IMessage* msg) = 0;
    };
  }  // namespace routing
}  // namespace llarp

#endif