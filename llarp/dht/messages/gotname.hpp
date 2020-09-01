#pragma once

#include <dht/message.hpp>

namespace llarp::dht
{
  struct GotNameMessage : public IMessage
  {
    explicit GotNameMessage(const Key_t& from, uint64_t txid, std::string data);

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) override;

    bool
    HandleMessage(struct llarp_dht_context* dht, std::vector<Ptr_t>& replies) const override;

    std::string Data;
    uint64_t TxID;
  };

}  // namespace llarp::dht
