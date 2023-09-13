#ifndef LLARP_DHT_LOCALSERVICEADDRESSLOOKUP
#define LLARP_DHT_LOCALSERVICEADDRESSLOOKUP

#include "serviceaddresslookup.hpp"

#include <llarp/path/path_types.hpp>

namespace llarp::dht
{
  struct LocalServiceAddressLookup : public ServiceAddressLookup
  {
    PathID_t localPath;

    LocalServiceAddressLookup(
        const PathID_t& pathid,
        uint64_t txid,
        uint64_t relayOrder,
        const Key_t& addr,
        AbstractDHTMessageHandler* ctx,
        [[maybe_unused]] const Key_t& askpeer);

    void
    SendReply() override;
  };

}  // namespace llarp::dht

#endif
