#ifndef LLARP_ROUTING_MESSAGE_HPP
#define LLARP_ROUTING_MESSAGE_HPP

#include <path/path_types.hpp>
#include <util/bencode.hpp>
#include <util/buffer.hpp>

namespace llarp
{
  struct AbstractRouter;
  namespace routing
  {
    struct IMessageHandler;

    struct IMessage : public llarp::IBEncodeMessage
    {
      PathID_t from;
      uint64_t S;

      IMessage() : llarp::IBEncodeMessage(), S(0)
      {
      }

      virtual ~IMessage(){};

      virtual bool
      HandleMessage(IMessageHandler* h, AbstractRouter* r) const = 0;

      virtual void
      Clear() = 0;
    };

  }  // namespace routing
}  // namespace llarp

#endif
