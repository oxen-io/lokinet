#ifndef LLARP_ROUTING_MESSAGE_HPP
#define LLARP_ROUTING_MESSAGE_HPP

#include <llarp/buffer.h>
#include <llarp/router.h>
#include <llarp/path_types.hpp>

namespace llarp
{
  namespace routing
  {
    struct IMessage
    {
      llarp::PathID_t from;

      virtual ~IMessage(){};

      virtual bool
      BEncode(llarp_buffer_t* buf) const = 0;

      virtual bool
      BDecode(llarp_buffer_t* buf) = 0;

      virtual bool
      HandleMessage(llarp_router* r) const = 0;
    };
  }  // namespace routing
}  // namespace llarp

#endif