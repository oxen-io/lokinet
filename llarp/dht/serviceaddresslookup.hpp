#ifndef LLARP_DHT_SERVICEADDRESSLOOKUP
#define LLARP_DHT_SERVICEADDRESSLOOKUP

#include <dht/key.hpp>
#include <dht/tx.hpp>
#include <service/address.hpp>
#include <service/intro_set.hpp>

namespace llarp
{
  namespace dht
  {
    struct TXOwner;

    struct ServiceAddressLookup : public TX< Key_t, service::EncryptedIntroSet >
    {
      service::EncryptedIntroSetLookupHandler handleResult;
      uint64_t R;

      ServiceAddressLookup(const TXOwner &asker, const Key_t &addr,
                           AbstractContext *ctx, uint64_t r,
                           service::EncryptedIntroSetLookupHandler handler);

      bool
      Validate(const service::EncryptedIntroSet &value) const override;

      bool
      GetNextPeer(Key_t &next, const std::set< Key_t > &exclude) override;

      void
      Start(const TXOwner &peer) override;

      void
      DoNextRequest(const Key_t &ask) override;

      void
      SendReply() override;
    };
  }  // namespace dht

}  // namespace llarp

#endif
