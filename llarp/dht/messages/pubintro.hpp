#ifndef LLARP_DHT_MESSAGES_PUB_INTRO_HPP
#define LLARP_DHT_MESSAGES_PUB_INTRO_HPP
#include <dht/message.hpp>
#include <service/intro_set.hpp>

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
      std::vector< Key_t > exclude;
      uint64_t depth = 0;
      uint64_t txID  = 0;
      PublishIntroMessage() : IMessage({})
      {
      }

      PublishIntroMessage(const llarp::service::EncryptedIntroSet& i,
                          uint64_t tx, uint64_t s,
                          std::vector< Key_t > _exclude = {})
          : IMessage({})
          , introset(i)
          , exclude(std::move(_exclude))
          , depth(s)
          , txID(tx)
      {
      }

      ~PublishIntroMessage() override;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) override;

      bool
      HandleMessage(
          llarp_dht_context* ctx,
          std::vector< std::unique_ptr< IMessage > >& replies) const override;
    };
  }  // namespace dht
}  // namespace llarp
#endif
