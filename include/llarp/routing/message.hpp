#ifndef LLARP_ROUTING_MESSAGE_HPP
#define LLARP_ROUTING_MESSAGE_HPP

#include <llarp/buffer.h>
#include <llarp/router.h>
#include <llarp/path_types.hpp>

namespace llarp
{
  namespace routing
  {
    struct IMessageHandler;

    struct IMessage
    {
      llarp::PathID_t from;

      virtual ~IMessage(){};

      virtual bool
      BEncode(llarp_buffer_t* buf) const = 0;

      virtual bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) = 0;

      virtual bool
      HandleMessage(IMessageHandler* r) const = 0;
    };

    struct InboundMessageParser
    {
      InboundMessageParser();
      bool
      ParseMessageBuffer(llarp_buffer_t buf, IMessageHandler* handler);

     private:
      static bool
      OnKey(dict_reader* r, llarp_buffer_t* key);
      bool firstKey;
      dict_reader reader;
      IMessage* msg;
    };
  }  // namespace routing
}  // namespace llarp

#endif