#ifndef LLARP_DHT_SERVICEADDRESSLOOKUP
#define LLARP_DHT_SERVICEADDRESSLOOKUP

#include "key.hpp"
#include "tx.hpp"
#include <llarp/service/address.hpp>
#include <llarp/service/intro_set.hpp>

namespace llarp
{
  namespace dht
  {
    struct TXOwner;

    struct ServiceAddressLookup : public TX<TXOwner, service::EncryptedIntroSet>
    {
      Key_t location;
      service::EncryptedIntroSetLookupHandler handleResult;
      uint32_t relayOrder;

      ServiceAddressLookup(
          const TXOwner& asker,
          const Key_t& addr,
          AbstractContext* ctx,
          uint32_t relayOrder,
          service::EncryptedIntroSetLookupHandler handler);

      bool
      Validate(const service::EncryptedIntroSet& value) const override;

      void
      Start(const TXOwner& peer) override;

      void
      SendReply() override;
    };
  }  // namespace dht

}  // namespace llarp

#endif
