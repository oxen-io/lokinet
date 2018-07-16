#ifndef LLARP_DHT_MESSAGES_FIND_INTRO_HPP
#define LLARP_DHT_MESSAGES_FIND_INTRO_HPP
#include <llarp/dht/message.hpp>
#include <llarp/service/address.hpp>

namespace llarp
{
  namespace dht
  {
    struct FindIntroMessage : public IMessage
    {
      uint64_t R     = 0;
      bool iterative = false;
      llarp::service::Address S;
      uint64_t T = 0;

      FindIntroMessage(const Key_t& from) : IMessage(from)
      {
      }

      virtual ~FindIntroMessage();

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val);

      virtual bool
      HandleMessage(llarp_dht_context* ctx,
                    std::vector< IMessage* >& replies) const;
    };

    struct RelayedFindIntroMessage : public FindIntroMessage
    {
      RelayedFindIntroMessage();
      ~RelayedFindIntroMessage();

      bool
      HandleMessage(llarp_dht_context* ctx,
                    std::vector< IMessage* >& replies) const;
    };
  }  // namespace dht
}  // namespace llarp
#endif