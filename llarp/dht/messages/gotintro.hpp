#ifndef LLARP_DHT_MESSAGES_GOT_INTRO_HPP
#define LLARP_DHT_MESSAGES_GOT_INTRO_HPP
#include <dht/message.hpp>
#include <llarp/service/IntroSet.hpp>
#include <vector>

namespace llarp
{
  namespace dht
  {
    /// acknowledgement to PublishIntroMessage or reply to FindIntroMessage
    struct GotIntroMessage : public IMessage
    {
      /// the found introsets
      std::vector< llarp::service::IntroSet > I;
      /// txid
      uint64_t T = 0;
      /// the key of a router closer in keyspace if iterative lookup
      std::unique_ptr< Key_t > K;

      GotIntroMessage(const Key_t& from) : IMessage(from)
      {
      }

      /// for iterative reply
      GotIntroMessage(const Key_t& from, const Key_t& closer, uint64_t txid)
          : IMessage(from), T(txid), K(new Key_t(closer))
      {
      }

      /// for recursive reply
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
