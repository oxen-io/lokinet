#ifndef LLARP_DHT_PUBLISHSERVICEJOB
#define LLARP_DHT_PUBLISHSERVICEJOB

#include <dht/tx.hpp>
#include <dht/txowner.hpp>
#include <service/address.hpp>
#include <service/intro_set.hpp>

#include <set>

namespace llarp
{
  namespace dht
  {
    struct PublishServiceJob : public TX< Key_t, service::EncryptedIntroSet >
    {
      bool relayed;
      uint64_t relayOrder;
      service::EncryptedIntroSet introset;

      PublishServiceJob(const TXOwner &asker,
                        const service::EncryptedIntroSet &introset,
                        AbstractContext *ctx, bool relayed,
                        uint64_t relayOrder);

      bool
      Validate(const service::EncryptedIntroSet &introset) const override;

      void
      Start(const TXOwner &peer) override;

      void
      SendReply() override
      {
        // don't need this
      }
    };
  }  // namespace dht
}  // namespace llarp

#endif
