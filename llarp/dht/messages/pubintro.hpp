#pragma once
#include <llarp/dht/message.hpp>
#include <llarp/service/intro_set.hpp>

#include <utility>
#include <vector>

namespace llarp::dht
{
  struct PublishIntroMessage final : public AbstractDHTMessage
  {
    static const uint64_t MaxPropagationDepth;
    llarp::service::EncryptedIntroSet introset;
    bool relayed = false;
    uint64_t relayOrder = 0;
    uint64_t txID = 0;
    PublishIntroMessage(const Key_t& from, bool relayed_)
        : AbstractDHTMessage(from), relayed(relayed_)
    {}

    PublishIntroMessage(
        const llarp::service::EncryptedIntroSet& introset_,
        uint64_t tx,
        bool relayed_,
        uint64_t relayOrder_)
        : AbstractDHTMessage({})
        , introset(introset_)
        , relayed(relayed_)
        , relayOrder(relayOrder_)
        , txID(tx)
    {}

    ~PublishIntroMessage() override;

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
