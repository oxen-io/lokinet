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
      std::list< llarp::service::IntroSet > I;
      uint64_t T;

      GotIntroMessage(uint64_t tx, const llarp::service::IntroSet* i = nullptr);

      ~GotIntroMessage();

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val);

      virtual bool
      HandleMessage(llarp_dht_context* ctx,
                    std::vector< IMessage* >& replies) const;
    };
  }
}
#endif