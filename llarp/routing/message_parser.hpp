#pragma once

#include <llarp/util/bencode.h>
#include <llarp/util/buffer.hpp>

#include <memory>

namespace llarp
{
  struct Router;
  struct PathID_t;

  namespace routing
  {
    struct AbstractRoutingMessage;
    struct AbstractRoutingMessageHandler;

    struct InboundMessageParser
    {
      InboundMessageParser();
      ~InboundMessageParser();

      bool
      ParseMessageBuffer(
          const llarp_buffer_t& buf,
          AbstractRoutingMessageHandler* handler,
          const PathID_t& from,
          Router* r);

      bool
      operator()(llarp_buffer_t* buffer, llarp_buffer_t* key);

     private:
      uint64_t version = 0;
      bool firstKey{false};
      char ourKey{'\0'};
      struct MessageHolder;

      AbstractRoutingMessage* msg{nullptr};
      std::unique_ptr<MessageHolder> m_Holder;
    };
  }  // namespace routing
}  // namespace llarp
