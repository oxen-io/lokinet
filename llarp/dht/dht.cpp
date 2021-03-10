#include "context.hpp"
#include "dht.h"
#include <llarp/router_contact.hpp>

llarp_dht_context::llarp_dht_context(llarp::AbstractRouter* router)
{
  parent = router;
  impl = llarp::dht::makeContext();
}

struct llarp_dht_context*
llarp_dht_context_new(llarp::AbstractRouter* router)
{
  return new llarp_dht_context(router);
}

void
llarp_dht_context_free(struct llarp_dht_context* ctx)
{
  delete ctx;
}

void
__llarp_dht_remove_peer(struct llarp_dht_context* ctx, const byte_t* id)
{
  ctx->impl->Nodes()->DelNode(llarp::dht::Key_t(id));
}

void
llarp_dht_allow_transit(llarp_dht_context* ctx)
{
  ctx->impl->AllowTransit() = true;
}

void
llarp_dht_context_start(struct llarp_dht_context* ctx, const byte_t* key)
{
  ctx->impl->Init(llarp::dht::Key_t(key), ctx->parent);
}

void
llarp_dht_lookup_router(struct llarp_dht_context* ctx, struct llarp_router_lookup_job* job)
{
  job->dht = ctx;
  job->found = false;
  job->result.Clear();
  // llarp_rc_clear(&job->result);
  llarp::LogError("implement me llarp_dht_lookup_router");
  /*
  ctx->parent->logic->queue_job(
                        {job, &llarp::dht::Context::queue_router_lookup});
  */
}
