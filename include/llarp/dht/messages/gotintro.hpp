#ifndef LLARP_DHT_MESSAGES_GOT_INTRO_HPP
#define LLARP_DHT_MESSAGES_GOT_INTRO_HPP
#include <llarp/dht/message.hpp>
#include <llarp/service/IntroSet.hpp>
#include <vector>

namespace llarp
{
  namespace dht
  {
    /// acknowledgement to PublishIntroMessage or reply to FinIntroMessage
    struct GotIntroMessage : public IMessage
    {
      std::vector< llarp::service::IntroSet > I;
      uint64_t T = 0;

      GotIntroMessage(const Key_t& from) : IMessage(from)
      {
      }

      GotIntroMessage(const std::vector< llarp::service::IntroSet >& results,
                      uint64_t txid);

      ~GotIntroMessage();

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val) override;

      virtual bool
      HandleMessage(
          llarp_dht_context* ctx,
          std::vector< std::unique_ptr< IMessage > >& replies) const override;
    };

    struct RelayedGotIntroMessage final : public GotIntroMessage
    {
      RelayedGotIntroMessage() : GotIntroMessage({})
      {
      }

      bool
      HandleMessage(
          llarp_dht_context* ctx,
          std::vector< std::unique_ptr< IMessage > >& replies) const override;
    };
  }  // namespace dht
}  // namespace llarp
#endif
