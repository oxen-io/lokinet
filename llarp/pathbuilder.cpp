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
    llarp::Debug("Generated keys for build");
  }

  void
  pathbuilder_start_build(void* user)
  {
    // select hops
    llarp_pathbuild_job* job = static_cast< llarp_pathbuild_job* >(user);
    size_t idx               = 0;
    while(idx < job->hops.numHops)
    {
      job->selectHop(job->router->nodedb, &job->hops.routers[idx], idx);
      ++idx;
    }

    // async generate keys
    AsyncPathKeyExchangeContext< llarp_pathbuild_job >* ctx =
        new AsyncPathKeyExchangeContext< llarp_pathbuild_job >(
            &job->router->crypto);

    ctx->AsyncGenerateKeys(new Path(&job->hops), job->router->logic,
                           job->router->tp, job, &pathbuilder_generated_keys);
    // free rc
    idx = 0;
    while(idx < job->hops.numHops)
    {
      llarp_rc_free(&job->hops.routers[idx]);
      ++idx;
    }
  }
}  // namespace llarp

llarp_pathbuilder_context::llarp_pathbuilder_context(
    llarp_router* p_router, struct llarp_dht_context* p_dht)
{
  this->router = p_router;
  this->dht    = p_dht;
}

extern "C"
{
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
    job->router = job->context->router;
    llarp_logic_queue_job(job->router->logic,
                          {job, &llarp::pathbuilder_start_build});
  }
}