#ifndef LLARP_DHT_MESSAGES_FIND_INTRO_HPP
#define LLARP_DHT_MESSAGES_FIND_INTRO_HPP
#include <llarp/dht/message.hpp>
#include <llarp/service/address.hpp>
#include <llarp/service/tag.hpp>

namespace llarp
{
  namespace dht
  {
    struct FindIntroMessage : public IMessage
    {
      uint64_t R = 0;
      llarp::service::Address S;
      llarp::service::Tag N;
      uint64_t T   = 0;
      bool relayed = false;

      FindIntroMessage(const Key_t& from, bool relay) : IMessage(from)
      {
        relayed = relay;
      }

      FindIntroMessage(const llarp::service::Tag& tag, uint64_t txid,
                       uint64_t r = 3)
          : IMessage({}), R(r), N(tag), T(txid)
      {
        S.Zero();
      }

      FindIntroMessage(uint64_t txid, const llarp::service::Address& addr,
                       uint64_t r)
          : IMessage({}), R(r), S(addr), T(txid)
      {
        N.Zero();
      }

      ~FindIntroMessage();

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val);

      bool
      HandleMessage(llarp_dht_context* ctx,
                    std::vector< std::unique_ptr< IMessage > >& replies) const;
    };
  }  // namespace dht
}  // namespace llarp
#endif
