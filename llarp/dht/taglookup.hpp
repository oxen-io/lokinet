#ifndef LLARP_DHT_TAGLOOKUP
#define LLARP_DHT_TAGLOOKUP

#include <dht/tx.hpp>
#include <service/IntroSet.hpp>
#include <service/tag.hpp>

namespace llarp
{
  namespace dht
  {
    struct TagLookup : public TX< service::Tag, service::IntroSet >
    {
      uint64_t R;
      TagLookup(const TXOwner &asker, const service::Tag &tag,
                AbstractContext *ctx, uint64_t r)
          : TX< service::Tag, service::IntroSet >(asker, tag, ctx), R(r)
      {
      }

      bool
      Validate(const service::IntroSet &introset) const override;

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
