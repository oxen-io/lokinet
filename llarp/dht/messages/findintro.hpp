#pragma once

#include <llarp/dht/message.hpp>
#include <llarp/routing/message.hpp>
#include <llarp/service/address.hpp>
#include <llarp/service/tag.hpp>

namespace llarp::dht
{
  struct FindIntroMessage final : public AbstractDHTMessage
  {
    Key_t location;
    llarp::service::Tag tagName;
    uint64_t txID = 0;
    bool relayed = false;
    uint64_t relayOrder = 0;

    FindIntroMessage(const Key_t& from, bool relay, uint64_t order) : AbstractDHTMessage(from)
    {
      relayed = relay;
      relayOrder = order;
    }

    FindIntroMessage(const llarp::service::Tag& tag, uint64_t txid)
        : AbstractDHTMessage({}), tagName(tag), txID(txid)
    {}

    explicit FindIntroMessage(uint64_t txid, const Key_t& addr, uint64_t order)
        : AbstractDHTMessage({}), location(addr), txID(txid), relayOrder(order)
    {
      tagName.Zero();
    }

    ~FindIntroMessage() override;

    void
    bt_encode(oxenc::bt_dict_producer& btdp) const override;

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* val) override;

    bool
    handle_message(
        llarp_dht_context* ctx,
        std::vector<std::unique_ptr<AbstractDHTMessage>>& replies) const override;
  };
}  // namespace llarp::dht
