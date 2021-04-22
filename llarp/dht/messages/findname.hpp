#pragma once

#include <llarp/dht/message.hpp>

namespace llarp::dht
{
  struct FindNameMessage : public IMessage
  {
    explicit FindNameMessage(const Key_t& from, Key_t namehash, uint64_t txid);

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) override;

    bool
    HandleMessage(struct llarp_dht_context* dht, std::vector<Ptr_t>& replies) const override;

    Key_t NameHash;
    uint64_t TxID;
  };

}  // namespace llarp::dht
