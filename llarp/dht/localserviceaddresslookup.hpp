#ifndef LLARP_DHT_LOCALSERVICEADDRESSLOOKUP
#define LLARP_DHT_LOCALSERVICEADDRESSLOOKUP

#include <dht/serviceaddresslookup.hpp>

#include <path/path_types.hpp>

namespace llarp
{
  namespace dht
  {
    struct LocalServiceAddressLookup : public ServiceAddressLookup
    {
      PathID_t localPath;

      LocalServiceAddressLookup(const PathID_t &pathid, uint64_t txid,
                                uint32_t relayOrder, const Key_t &addr,
                                AbstractContext *ctx,
                                __attribute__((unused)) const Key_t &askpeer);

      void
      SendReply() override;
    };

  }  // namespace dht
}  // namespace llarp

#endif
