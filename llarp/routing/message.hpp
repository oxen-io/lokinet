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
    };

    struct InboundMessageParser
    {
      InboundMessageParser();
      bool
      ParseMessageBuffer(llarp_buffer_t buf, IMessageHandler* handler,
                         const PathID_t& from, llarp::Router* r);

     private:
      static bool
      OnKey(dict_reader* r, llarp_buffer_t* key);
      bool firstKey;
      char key;
      dict_reader reader;
      std::unique_ptr< IMessage > msg;
    };
  }  // namespace routing
}  // namespace llarp

#endif
