#ifndef LLARP_DHT_RECURSIVEROUTERLOOKUP
#define LLARP_DHT_RECURSIVEROUTERLOOKUP

#include "tx.hpp"

#include <llarp/router_contact.hpp>
#include <llarp/router_id.hpp>

namespace llarp
{
  namespace dht
  {
    struct RecursiveRouterLookup : public TX<RouterID, RouterContact>
    {
      RouterLookupHandler resultHandler;
      RecursiveRouterLookup(
          const TXOwner& whoasked,
          const RouterID& target,
          AbstractContext* ctx,
          RouterLookupHandler result);

      bool
      Validate(const RouterContact& rc) const override;

      void
      Start(const TXOwner& peer) override;

      void
      SendReply() override;
    };
  }  // namespace dht
}  // namespace llarp

#endif
