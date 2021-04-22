#ifndef LLARP_DHT_LOCALROUTERLOOKUP
#define LLARP_DHT_LOCALROUTERLOOKUP

#include "recursiverouterlookup.hpp"

#include <llarp/path/path_types.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/router_id.hpp>

namespace llarp
{
  namespace dht
  {
    struct LocalRouterLookup : public RecursiveRouterLookup
    {
      PathID_t localPath;

      LocalRouterLookup(
          const PathID_t& path, uint64_t txid, const RouterID& target, AbstractContext* ctx);

      void
      SendReply() override;
    };
  }  // namespace dht
}  // namespace llarp

#endif
