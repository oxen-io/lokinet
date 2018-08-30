#include <llarp/dht.h>
#include "router.hpp"
#include "router_contact.hpp"

llarp_dht_context::llarp_dht_context(llarp_router *router)
{
  parent = router;
}

struct llarp_dht_context *
llarp_dht_context_new(struct llarp_router *router)
{
  return new llarp_dht_context(router);
}

void
llarp_dht_context_free(struct llarp_dht_context *ctx)
{
  delete ctx;
}

void
__llarp_dht_remove_peer(struct llarp_dht_context *ctx, const byte_t *id)
{
  llarp::dht::Key_t k = id;
  llarp::LogDebug("Removing ", k, " to DHT");
  ctx->impl.nodes->DelNode(k);
}

void
llarp_dht_allow_transit(llarp_dht_context *ctx)
{
  ctx->impl.allowTransit = true;
}

void
llarp_dht_context_start(struct llarp_dht_context *ctx, const byte_t *key)
{
  ctx->impl.Init(key, ctx->parent, 20000);
}
