#pragma once

#include <llarp/dht/message.hpp>

namespace llarp::dht
{
  struct FindNameMessage : public AbstractDHTMessage
  {
    explicit FindNameMessage(const Key_t& from, Key_t namehash, uint64_t txid);

    void
    bt_encode(oxenc::bt_dict_producer& btdp) const override;

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* val) override;

    bool
    handle_message(struct llarp_dht_context* dht, std::vector<Ptr_t>& replies) const override;

    Key_t NameHash;
    uint64_t TxID;
  };

}  // namespace llarp::dht
