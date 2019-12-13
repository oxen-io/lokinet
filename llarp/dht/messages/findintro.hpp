#ifndef LLARP_DHT_MESSAGES_FIND_INTRO_HPP
#define LLARP_DHT_MESSAGES_FIND_INTRO_HPP

#include <dht/message.hpp>
#include <routing/message.hpp>
#include <service/address.hpp>
#include <service/tag.hpp>

namespace llarp
{
  namespace dht
  {
    struct FindIntroMessage final : public IMessage
    {
      uint64_t R = 0;
      llarp::service::Address S;
      llarp::service::Tag N;
      uint64_t T   = 0;
      bool relayed = false;
      std::string Name;

      FindIntroMessage(const Key_t& from, bool relay) : IMessage(from)
      {
        relayed = relay;
      }

      FindIntroMessage(uint64_t txid, const std::string name)
          : IMessage({}), T(txid), Name(name)
      {
      }

      FindIntroMessage(const llarp::service::Tag& tag, uint64_t txid,
                       bool iterate = true)
          : IMessage({}), N(tag), T(txid)
      {
        S.Zero();
        if(iterate)
          R = 0;
        else
          R = 1;
      }

      FindIntroMessage(uint64_t txid, const llarp::service::Address& addr,
                       bool iterate = true)
          : IMessage({}), S(addr), T(txid)
      {
        N.Zero();
        if(iterate)
          R = 0;
        else
          R = 1;
      }

      ~FindIntroMessage() override;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) override;

      bool
      HandleMessage(llarp_dht_context* ctx,
                    std::vector< IMessage::Ptr_t >& replies) const override;
    };
  }  // namespace dht
}  // namespace llarp
#endif
