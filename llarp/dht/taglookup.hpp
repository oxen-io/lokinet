#ifndef LLARP_DHT_TAGLOOKUP
#define LLARP_DHT_TAGLOOKUP

#include <dht/tx.hpp>
#include <service/intro_set.hpp>
#include <service/tag.hpp>

namespace llarp
{
  namespace dht
  {
    struct TagLookup : public TX< service::Tag, service::EncryptedIntroSet >
    {
      uint64_t recursionDepth;
      TagLookup(const TXOwner &asker, const service::Tag &tag,
                AbstractContext *ctx, uint64_t recursion)
          : TX< service::Tag, service::EncryptedIntroSet >(asker, tag, ctx)
          , recursionDepth(recursion)
      {
      }

      bool
      Validate(const service::EncryptedIntroSet &introset) const override;

      void
      Start(const TXOwner &peer) override;

      bool
      GetNextPeer(Key_t &, const std::set< Key_t > &) override
      {
        return false;
      }

      void
      DoNextRequest(const Key_t &) override
      {
      }

      void
      SendReply() override;
    };
  }  // namespace dht
}  // namespace llarp

#endif
