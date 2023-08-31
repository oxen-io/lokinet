#pragma once

#include <llarp/dht/message.hpp>
#include <llarp/service/name.hpp>

namespace llarp::dht
{
  struct GotNameMessage : public AbstractDHTMessage
  {
    explicit GotNameMessage(const Key_t& from, uint64_t txid, service::EncryptedName data);

    void
    bt_encode(oxenc::bt_dict_producer& btdp) const override;

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* val) override;

    bool
    handle_message(
        struct llarp_dht_context* dht,
        std::vector<std::unique_ptr<AbstractDHTMessage>>& replies) const override;

    service::EncryptedName result;
    uint64_t TxID;
  };
}  // namespace llarp::dht
