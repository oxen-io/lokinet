#ifndef LLARP_DHT_MESSAGES_GOT_INTRO_HPP
#define LLARP_DHT_MESSAGES_GOT_INTRO_HPP
#include <llarp/dht/message.hpp>
#include <llarp/service/IntroSet.hpp>

namespace llarp
{
  namespace dht
  {
    /// acknologement to PublishIntroMessage or reply to FinIntroMessage
    struct GotIntroMessage : public IMessage
    {
      std::set< llarp::service::IntroSet > I;
      uint64_t T = 0;

      GotIntroMessage(const Key_t& from) : IMessage(from)
      {
      }

      GotIntroMessage(const std::set< llarp::service::IntroSet >& results,
                      uint64_t txid);

      ~GotIntroMessage();

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val);

      virtual bool
      HandleMessage(llarp_dht_context* ctx,
                    std::vector< IMessage* >& replies) const;
    };

    struct RelayedGotIntroMessage : public GotIntroMessage
    {
      RelayedGotIntroMessage() : GotIntroMessage({})
      {
      }

      bool
      HandleMessage(llarp_dht_context* ctx,
                    std::vector< IMessage* >& replies) const;
    };
  }  // namespace dht
}  // namespace llarp
#endif