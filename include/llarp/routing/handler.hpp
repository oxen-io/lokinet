#ifndef LLARP_ROUTING_HANDLER_HPP
#define LLARP_ROUTING_HANDLER_HPP

#include <llarp/buffer.h>
#include <llarp/router.h>
#include <llarp/dht.hpp>
#include <llarp/messages/hidden_service.hpp>
#include <llarp/messages/path_confirm.hpp>
#include <llarp/messages/path_latency.hpp>
#include <llarp/messages/path_transfer.hpp>

namespace llarp
{
  namespace routing
  {
    // handles messages on the routing level
    struct IMessageHandler
    {
      virtual bool
      HandlePathTransferMessage(const PathTransferMessage *msg,
                                llarp_router *r) = 0;

      virtual bool
      HandleHiddenServiceFrame(const HiddenServiceFrame *msg)
      {
        return false;
      }

      virtual bool
      HandlePathConfirmMessage(const PathConfirmMessage *msg,
                               llarp_router *r) = 0;

      virtual bool
      HandlePathLatencyMessage(const PathLatencyMessage *msg,
                               llarp_router *r) = 0;
      virtual bool
      HandleDHTMessage(const llarp::dht::IMessage *msg, llarp_router *r) = 0;
    };
  }  // namespace routing
}  // namespace llarp

#endif