#pragma once
#include <llarp/dht/message.hpp>

namespace llarp
{
  namespace dht
  {
    struct FindRouterMessage : public IMessage
    {
      // inbound parsing
      FindRouterMessage(const Key_t& from) : IMessage(from)
      {}

      // find by routerid
      FindRouterMessage(uint64_t id, const RouterID& target)
          : IMessage({}), targetKey(target), txid(id)
      {}

      // exploritory
      FindRouterMessage(uint64_t id) : IMessage({}), exploritory(true), txid(id)
      {
        targetKey.Randomize();
      }

      ~FindRouterMessage() override;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) override;

      bool
      HandleMessage(
          llarp_dht_context* ctx, std::vector<std::unique_ptr<IMessage>>& replies) const override;

      RouterID targetKey;
      bool iterative = false;
      bool exploritory = false;
      uint64_t txid = 0;
      uint64_t version = 0;
    };

    /// variant of FindRouterMessage relayed via path
    struct RelayedFindRouterMessage final : public FindRouterMessage
    {
      RelayedFindRouterMessage(const Key_t& from) : FindRouterMessage(from)
      {}

      /// handle a relayed FindRouterMessage, do a lookup on the dht and inform
      /// the path of the result
      /// TODO: smart path expiration logic needs to be implemented
      bool
      HandleMessage(llarp_dht_context* ctx, std::vector<IMessage::Ptr_t>& replies) const override;
    };
  }  // namespace dht
}  // namespace llarp
