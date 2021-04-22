#ifndef LLARP_DHT_TAGLOOKUP
#define LLARP_DHT_TAGLOOKUP

#include "tx.hpp"
#include <llarp/service/intro_set.hpp>
#include <llarp/service/tag.hpp>

namespace llarp
{
  namespace dht
  {
    struct TagLookup : public TX<service::Tag, service::EncryptedIntroSet>
    {
      uint64_t recursionDepth;
      TagLookup(
          const TXOwner& asker, const service::Tag& tag, AbstractContext* ctx, uint64_t recursion)
          : TX<service::Tag, service::EncryptedIntroSet>(asker, tag, ctx), recursionDepth(recursion)
      {}

      bool
      Validate(const service::EncryptedIntroSet& introset) const override;

      void
      Start(const TXOwner& peer) override;

      void
      SendReply() override;
    };
  }  // namespace dht
}  // namespace llarp

#endif
