#ifndef LLARP_DHT_MESSAGES_FIND_ROUTER_HPP
#define LLARP_DHT_MESSAGES_FIND_ROUTER_HPP
#include <llarp/dht/message.hpp>

namespace llarp
{
  namespace dht
  {
    struct FindRouterMessage : public IMessage
    {
      FindRouterMessage(const Key_t& from) : IMessage(from)
      {
      }

      FindRouterMessage(const Key_t& from, const RouterID& target, uint64_t id)
          : IMessage(from), K(target), txid(id)
      {
      }

      // exploritory
      FindRouterMessage(const Key_t& from, uint64_t id)
          : IMessage(from), exploritory(true), txid(id)
      {
        K.Randomize();
      }

      ~FindRouterMessage();

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val);

      virtual bool
      HandleMessage(llarp_dht_context* ctx,
                    std::vector< std::unique_ptr< IMessage > >& replies) const;

      RouterID K;
      bool iterative   = false;
      bool exploritory = false;
      uint64_t txid    = 0;
      uint64_t version = 0;
    };

    /// variant of FindRouterMessage relayed via path
    struct RelayedFindRouterMessage : public FindRouterMessage
    {
      RelayedFindRouterMessage(const Key_t& from) : FindRouterMessage(from)
      {
      }

      /// handle a relayed FindRouterMessage, do a lookup on the dht and inform
      /// the path of the result
      /// TODO: smart path expiration logic needs to be implemented
      virtual bool
      HandleMessage(llarp_dht_context* ctx,
                    std::vector< std::unique_ptr< IMessage > >& replies) const;
    };
  }  // namespace dht
}  // namespace llarp
#endif
