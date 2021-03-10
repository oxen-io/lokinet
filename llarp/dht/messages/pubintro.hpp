#pragma once
#include <llarp/dht/message.hpp>
#include <llarp/service/intro_set.hpp>

#include <utility>
#include <vector>

namespace llarp
{
  namespace dht
  {
    struct PublishIntroMessage final : public IMessage
    {
      static const uint64_t MaxPropagationDepth;
      llarp::service::EncryptedIntroSet introset;
      bool relayed = false;
      uint64_t relayOrder = 0;
      uint64_t txID = 0;
      PublishIntroMessage(const Key_t& from, bool relayed_) : IMessage(from), relayed(relayed_)
      {}

      PublishIntroMessage(
          const llarp::service::EncryptedIntroSet& introset_,
          uint64_t tx,
          bool relayed_,
          uint64_t relayOrder_)
          : IMessage({}), introset(introset_), relayed(relayed_), relayOrder(relayOrder_), txID(tx)
      {}

      ~PublishIntroMessage() override;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) override;

      bool
      HandleMessage(
          llarp_dht_context* ctx, std::vector<std::unique_ptr<IMessage>>& replies) const override;
    };
  }  // namespace dht
}  // namespace llarp
