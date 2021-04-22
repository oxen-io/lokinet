#pragma once

#include <llarp/util/bencode.h>
#include <llarp/util/buffer.hpp>

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
      ParseMessageBuffer(
          const llarp_buffer_t& buf,
          IMessageHandler* handler,
          const PathID_t& from,
          AbstractRouter* r);

      bool
      operator()(llarp_buffer_t* buffer, llarp_buffer_t* key);

     private:
      uint64_t version = 0;
      bool firstKey{false};
      char ourKey{'\0'};
      struct MessageHolder;

      IMessage* msg{nullptr};
      std::unique_ptr<MessageHolder> m_Holder;
    };
  }  // namespace routing
}  // namespace llarp
