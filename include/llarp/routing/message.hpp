#ifndef LLARP_ROUTING_MESSAGE_HPP
#define LLARP_ROUTING_MESSAGE_HPP

#include <llarp/buffer.h>
#include <llarp/router.h>
#include <llarp/bencode.hpp>
#include <llarp/path_types.hpp>

namespace llarp
{
  namespace routing
  {
    struct IMessageHandler;

    struct IMessage : public llarp::IBEncodeMessage
    {
      llarp::PathID_t from;

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