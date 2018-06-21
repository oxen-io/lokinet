#include <llarp/nodedb.h>
#include <llarp/path.hpp>

#include "pathbuilder.hpp"
#include "router.hpp"

namespace llarp
{
  PathHopConfig::PathHopConfig()
  {
    llarp_rc_clear(&router);
  }

  PathHopConfig::~PathHopConfig()
  {
    llarp_rc_free(&router);
  }

  void
  pathbuilder_generated_keys(
      AsyncPathKeyExchangeContext< llarp_pathbuild_job >* ctx)
  {
    auto remote = ctx->path->Upstream();
    llarp::Debug("Generated LRCM to", remote);
    auto router = ctx->user->router;
    if(!router->SendToOrQueue(remote, ctx->LRCM))
    {
      llarp::Error("failed to send LRCM");
      return;
    }
    ctx->path->status = ePathBuilding;
    router->paths.AddOwnPath(ctx->path);
    ctx->user->pathBuildStarted(ctx->user);
  }

  void
  pathbuilder_start_build(void* user)
  {
    llarp_pathbuild_job* job = static_cast< llarp_pathbuild_job* >(user);
    // select hops
    size_t idx     = 0;
    llarp_rc* prev = nullptr;
    while(idx < job->hops.numHops)
    {
      llarp_rc* rc = &job->hops.hops[idx].router;
      llarp_rc_clear(rc);
      job->selectHop(job->router->nodedb, prev, rc, idx);
      prev = rc;
      ++idx;
    }

    // async generate keys
    AsyncPathKeyExchangeContext< llarp_pathbuild_job >* ctx =
        new AsyncPathKeyExchangeContext< llarp_pathbuild_job >(
            &job->router->crypto);

    ctx->AsyncGenerateKeys(new Path(&job->hops), job->router->logic,
                           job->router->tp, job, &pathbuilder_generated_keys);
  }
}  // namespace llarp

llarp_pathbuilder_context::llarp_pathbuilder_context(
    llarp_router* p_router, struct llarp_dht_context* p_dht)
    : router(p_router), dht(p_dht)
{
}

extern "C" {
struct llarp_pathbuilder_context*
llarp_pathbuilder_context_new(struct llarp_router* router,
                              struct llarp_dht_context* dht)
{
  return new llarp_pathbuilder_context(router, dht);
}

void
llarp_pathbuilder_context_free(struct llarp_pathbuilder_context* ctx)
{
  delete ctx;
}

void
llarp_pathbuilder_build_path(struct llarp_pathbuild_job* job)
{
  if (!job->context)
  {
    llarp::Error("failed to build path because no context is set in job");
    return;
  }
  job->router = job->context->router;
  if(job->selectHop == nullptr)
    job->selectHop = &llarp_nodedb_select_random_hop;
  llarp_logic_queue_job(job->router->logic,
                        {job, &llarp::pathbuilder_start_build});
}
}
