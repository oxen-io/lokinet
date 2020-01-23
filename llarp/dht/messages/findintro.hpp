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
      uint64_t recursionDepth = 0;
      llarp::service::Address serviceAddress;
      llarp::service::Tag tagName;
      uint64_t txID = 0;
      bool relayed  = false;

      FindIntroMessage(const Key_t& from, bool relay) : IMessage(from)
      {
        relayed = relay;
      }

      FindIntroMessage(const llarp::service::Tag& tag, uint64_t txid,
                       bool iterate = true)
          : IMessage({}), tagName(tag), txID(txid)
      {
        serviceAddress.Zero();
        if(iterate)
          recursionDepth = 0;
        else
          recursionDepth = 1;
      }

      FindIntroMessage(uint64_t txid, const llarp::service::Address& addr,
                       uint64_t maxRecursionDepth)
          : IMessage({})
          , recursionDepth(maxRecursionDepth)
          , serviceAddress(addr)
          , txID(txid)
      {
        tagName.Zero();
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
