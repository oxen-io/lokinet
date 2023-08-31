#ifndef LLARP_DHT_LOOKUPTAGLOOKUP
#define LLARP_DHT_LOOKUPTAGLOOKUP

#include "taglookup.hpp"

namespace llarp::dht
{
  struct LocalTagLookup : public TagLookup
  {
    PathID_t localPath;

    LocalTagLookup(
        const PathID_t& path, uint64_t txid, const service::Tag& target, AbstractContext* ctx);

    void
    SendReply() override;
  };
}  // namespace llarp::dht

#endif
