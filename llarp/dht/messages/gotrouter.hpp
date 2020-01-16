#ifndef LLARP_DHT_MESSAGES_GOT_ROUTER_HPP
#define LLARP_DHT_MESSAGES_GOT_ROUTER_HPP

#include <dht/message.hpp>
#include <router_contact.hpp>
#include <util/copy_or_nullptr.hpp>
#include <utility>
#include <vector>

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

      GotRouterMessage(uint64_t id, std::vector< RouterID > _near,
                       bool tunneled)
          : IMessage({}), N(std::move(_near)), txid(id), relayed(tunneled)
      {
      }

      GotRouterMessage(const GotRouterMessage& other)
          : IMessage(other.From)
          , R(other.R)
          , N(other.N)
          , K(copy_or_nullptr(other.K))
          , txid(other.txid)
          , relayed(other.relayed)
      {
        version = other.version;
      }

      ~GotRouterMessage() override;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) override;

      bool
      HandleMessage(
          llarp_dht_context* ctx,
          std::vector< std::unique_ptr< IMessage > >& replies) const override;

      std::vector< RouterContact > R;
      std::vector< RouterID > N;
      std::unique_ptr< Key_t > K;
      uint64_t txid = 0;
      bool relayed  = false;
    };

    using GotRouterMessage_constptr = std::shared_ptr< const GotRouterMessage >;
  }  // namespace dht
}  // namespace llarp
#endif
