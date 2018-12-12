#ifndef LLARP_MESSAGES_DHT_HPP
#define LLARP_MESSAGES_DHT_HPP
#include <dht.hpp>
#include <llarp/routing/message.hpp>

#include <vector>

namespace llarp
{
  namespace routing
  {
    struct DHTMessage final : public IMessage
    {
      std::vector< std::unique_ptr< llarp::dht::IMessage > > M;
      uint64_t V = 0;

      ~DHTMessage();

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val) override;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      HandleMessage(IMessageHandler* h, llarp::Router* r) const override;
    };
  }  // namespace routing
}  // namespace llarp

#endif
