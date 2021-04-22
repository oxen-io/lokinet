#ifndef LLARP_DHT_PUBLISHSERVICEJOB
#define LLARP_DHT_PUBLISHSERVICEJOB

#include "tx.hpp"
#include "txowner.hpp"
#include <llarp/service/address.hpp>
#include <llarp/service/intro_set.hpp>

#include <set>

namespace llarp
{
  namespace dht
  {
    struct PublishServiceJob : public TX<TXOwner, service::EncryptedIntroSet>
    {
      uint64_t relayOrder;
      service::EncryptedIntroSet introset;

      PublishServiceJob(
          const TXOwner& asker,
          const service::EncryptedIntroSet& introset,
          AbstractContext* ctx,
          uint64_t relayOrder);

      bool
      Validate(const service::EncryptedIntroSet& introset) const override;

      void
      Start(const TXOwner& peer) override;

      virtual void
      SendReply() override;
    };

    struct LocalPublishServiceJob : public PublishServiceJob
    {
      PathID_t localPath;
      uint64_t txid;
      LocalPublishServiceJob(
          const TXOwner& peer,
          const PathID_t& fromID,
          uint64_t txid,
          const service::EncryptedIntroSet& introset,
          AbstractContext* ctx,
          uint64_t relayOrder);

      void
      SendReply() override;
    };
  }  // namespace dht
}  // namespace llarp

#endif
