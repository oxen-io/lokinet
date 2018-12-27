#ifndef LLARP_ROUTING_MESSAGE_HPP
#define LLARP_ROUTING_MESSAGE_HPP

#include <bencode.hpp>
#include <buffer.h>
#include <path_types.hpp>

namespace llarp
{
  struct Router;
  namespace routing
  {
    struct IMessageHandler;

    struct IMessage : public llarp::IBEncodeMessage
    {
      llarp::PathID_t from;
      uint64_t S = 0;

      IMessage() : llarp::IBEncodeMessage()
      {
      }

      virtual ~IMessage(){};

      virtual bool
      HandleMessage(IMessageHandler* h, llarp::Router* r) const = 0;

      virtual void 
      Clear() = 0;
    };

    
  }  // namespace routing
}  // namespace llarp

#endif
