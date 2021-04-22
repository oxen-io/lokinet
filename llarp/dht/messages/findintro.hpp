#pragma once

#include <llarp/dht/message.hpp>
#include <llarp/routing/message.hpp>
#include <llarp/service/address.hpp>
#include <llarp/service/tag.hpp>

namespace llarp
{
  namespace dht
  {
    struct FindIntroMessage final : public IMessage
    {
      Key_t location;
      llarp::service::Tag tagName;
      uint64_t txID = 0;
      bool relayed = false;
      uint64_t relayOrder = 0;

      FindIntroMessage(const Key_t& from, bool relay, uint64_t order) : IMessage(from)
      {
        relayed = relay;
        relayOrder = order;
      }

      FindIntroMessage(const llarp::service::Tag& tag, uint64_t txid)
          : IMessage({}), tagName(tag), txID(txid)
      {}

      explicit FindIntroMessage(uint64_t txid, const Key_t& addr, uint64_t order)
          : IMessage({}), location(addr), txID(txid), relayOrder(order)
      {
        tagName.Zero();
      }

      ~FindIntroMessage() override;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) override;

      bool
      HandleMessage(llarp_dht_context* ctx, std::vector<IMessage::Ptr_t>& replies) const override;
    };
  }  // namespace dht
}  // namespace llarp
