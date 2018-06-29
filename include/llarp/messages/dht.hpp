#ifndef LLARP_MESSAGES_DHT_HPP
#define LLARP_MESSAGES_DHT_HPP
#include <llarp/dht.hpp>
#include <llarp/routing/message.hpp>

#include <vector>

namespace llarp
{
  namespace routing
  {
    struct DHTMessage : public IMessage
    {
      std::vector< llarp::dht::IMessage* > M;
      uint64_t V = 0;

      ~DHTMessage();

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val);

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      HandleMessage(IMessageHandler* h, llarp_router* r) const;
    };
  }  // namespace routing
}  // namespace llarp

#endif