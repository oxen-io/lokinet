#ifndef LLARP_DHT_MESSAGES_GOT_ROUTER_HPP
#define LLARP_DHT_MESSAGES_GOT_ROUTER_HPP
#include <llarp/dht/message.hpp>
#include <llarp/router_contact.hpp>

namespace llarp
{
  namespace dht
  {
    struct GotRouterMessage final : public IMessage
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

      GotRouterMessage(const Key_t& from, const Key_t& closer, uint64_t id,
                       bool tunneled)
          : IMessage(from), K(new Key_t(closer)), txid(id), relayed(tunneled)
      {
      }

      GotRouterMessage(uint64_t id, const std::vector< RouterID >& near,
                       bool tunneled)
          : IMessage({}), N(near), txid(id), relayed(tunneled)
      {
      }

      ~GotRouterMessage();

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val) override;

      virtual bool
      HandleMessage(
          llarp_dht_context* ctx,
          std::vector< std::unique_ptr< IMessage > >& replies) const override;

      std::vector< RouterContact > R;
      std::vector< RouterID > N;
      std::unique_ptr< Key_t > K;
      uint64_t txid    = 0;
      uint64_t version = 0;
      bool relayed     = false;
    };
  }  // namespace dht
}  // namespace llarp
#endif
