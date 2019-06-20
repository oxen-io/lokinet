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
      llarp::service::IntroSet I;
      std::vector< Key_t > E;
      uint64_t R    = 0;
      uint64_t S    = 0;
      uint64_t txID = 0;
      bool hasS     = false;
      PublishIntroMessage() : IMessage({})
      {
      }

      PublishIntroMessage(const llarp::service::IntroSet& i, uint64_t tx,
                          uint64_t s, std::vector< Key_t > exclude = {})
          : IMessage({}), E(std::move(exclude)), txID(tx)
      {
        I = i;
        S = s;
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
