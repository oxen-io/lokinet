#ifndef LLARP_DHT_MESSAGES_GOT_ROUTER_HPP
#define LLARP_DHT_MESSAGES_GOT_ROUTER_HPP
#include <llarp/dht/message.hpp>
#include <llarp/router_contact.hpp>

namespace llarp
{
  namespace dht
  {
    struct GotRouterMessage : public IMessage
    {
      GotRouterMessage(const Key_t& from, bool tunneled)
          : IMessage(from), relayed(tunneled)
      {
      }
      GotRouterMessage(const Key_t& from, uint64_t id,
                       const std::vector< RouterContact >& results,
                       bool tunneled)
          : IMessage(from), R(results), txid(id), relayed(tunneled)
      {
      }

      GotRouterMessage(uint64_t id, const std::vector< RouterID >& near,
                       bool tunneled)
          : IMessage({}), N(near), txid(id), relayed(tunneled)
      {
      }

      ~GotRouterMessage();

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val);

      virtual bool
      HandleMessage(llarp_dht_context* ctx,
                    std::vector< IMessage* >& replies) const;

      std::vector< RouterContact > R;
      std::vector< RouterID > N;
      uint64_t txid    = 0;
      uint64_t version = 0;
      bool relayed     = false;
    };
  }  // namespace dht
}  // namespace llarp
#endif
