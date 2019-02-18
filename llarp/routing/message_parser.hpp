#ifndef LLARP_ROUTING_MESSAGE_PARSER_HPP
#define LLARP_ROUTING_MESSAGE_PARSER_HPP

#include <util/bencode.h>
#include <util/buffer.hpp>

#include <memory>

namespace llarp
{
  struct AbstractRouter;
  struct PathID_t;

  namespace routing
  {
    struct IMessage;
    struct IMessageHandler;

    struct InboundMessageParser
    {
      InboundMessageParser();
      ~InboundMessageParser();

      bool
      ParseMessageBuffer(const llarp_buffer_t& buf, IMessageHandler* handler,
                         const PathID_t& from, AbstractRouter* r);

     private:
      static bool
      OnKey(dict_reader* r, llarp_buffer_t* key);

      bool firstKey;
      char key;
      dict_reader reader;

      struct MessageHolder;

      IMessage* msg;
      std::unique_ptr< MessageHolder > m_Holder;
    };
  }  // namespace routing
}  // namespace llarp
#endif
