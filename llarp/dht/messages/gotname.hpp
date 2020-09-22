#pragma once

#include <dht/message.hpp>
#include <service/name.hpp>

namespace llarp::dht
{
  struct GotNameMessage : public IMessage
  {
    explicit GotNameMessage(const Key_t& from, uint64_t txid, service::EncryptedName data);

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) override;

    bool
    HandleMessage(struct llarp_dht_context* dht, std::vector<Ptr_t>& replies) const override;

    service::EncryptedName result;
    uint64_t TxID;
  };

}  // namespace llarp::dht
