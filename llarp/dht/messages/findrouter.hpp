#pragma once
#include <llarp/dht/message.hpp>

namespace llarp::dht
{
  struct FindRouterMessage : public AbstractDHTMessage
  {
    // inbound parsing
    FindRouterMessage(const Key_t& from) : AbstractDHTMessage(from)
    {}

    // find by routerid
    FindRouterMessage(uint64_t id, const RouterID& target)
        : AbstractDHTMessage({}), targetKey(target), txid(id)
    {}

    // exploritory
    FindRouterMessage(uint64_t id) : AbstractDHTMessage({}), exploratory(true), txid(id)
    {
      targetKey.Randomize();
    }

    ~FindRouterMessage() override;

    void
    bt_encode(oxenc::bt_dict_producer& btdp) const override;

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* val) override;

    bool
    handle_message(
        AbstractDHTMessageHandler& dht,
        std::vector<std::unique_ptr<AbstractDHTMessage>>& replies) const override;

    RouterID targetKey;
    bool iterative = false;
    bool exploratory = false;
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
    handle_message(
        AbstractDHTMessageHandler& dht,
        std::vector<std::unique_ptr<AbstractDHTMessage>>& replies) const override;
  };
}  // namespace llarp::dht
