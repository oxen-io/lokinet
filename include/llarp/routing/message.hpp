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
      uint64_t S = 0;

      IMessage() : llarp::IBEncodeMessage()
      {
      }

      virtual ~IMessage(){};

      virtual bool
      HandleMessage(IMessageHandler* h, llarp_router* r) const = 0;
    };

    struct InboundMessageParser
    {
      InboundMessageParser();
      bool
      ParseMessageBuffer(llarp_buffer_t buf, IMessageHandler* handler,
                         const PathID_t& from, llarp_router* r);

     private:
      static bool
      OnKey(dict_reader* r, llarp_buffer_t* key);
      bool firstKey;
      char key;
      dict_reader reader;
      std::unique_ptr<IMessage> msg;
    };
  }  // namespace routing
}  // namespace llarp

#endif
