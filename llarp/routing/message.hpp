#ifndef LLARP_ROUTING_MESSAGE_HPP
#define LLARP_ROUTING_MESSAGE_HPP

#include <constants/proto.hpp>
#include <path/path_types.hpp>
#include <util/bencode.hpp>
#include <util/buffer.hpp>

namespace llarp
{
  struct AbstractRouter;
  namespace routing
  {
    struct IMessageHandler;

    struct IMessage
    {
      PathID_t from;
      uint64_t S{0};
      uint64_t version = LLARP_PROTO_VERSION;

      IMessage() = default;

      virtual ~IMessage() = default;

      virtual bool
      BEncode(llarp_buffer_t* buf) const = 0;

      virtual bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) = 0;

      virtual bool
      HandleMessage(IMessageHandler* h, AbstractRouter* r) const = 0;

      virtual void
      Clear() = 0;
    };

  }  // namespace routing
}  // namespace llarp

#endif
