#include <llarp/nodedb.h>
#include <llarp/path.hpp>

#include "pathbuilder.hpp"
#include "router.hpp"

namespace llarp
{
  template < typename User >
  struct AsyncPathKeyExchangeContext
  {
    Path* path = nullptr;
    typedef void (*Handler)(AsyncPathKeyExchangeContext< User >*);
    User* user               = nullptr;
    Handler result           = nullptr;
    size_t idx               = 0;
    llarp_threadpool* worker = nullptr;
    llarp_logic* logic       = nullptr;
    llarp_crypto* crypto     = nullptr;
    LR_CommitMessage* LRCM   = nullptr;

    static void
    HandleDone(void* u)
    {
      AsyncPathKeyExchangeContext< User >* ctx =
          static_cast< AsyncPathKeyExchangeContext< User >* >(u);
      ctx->result(ctx);
    }

    static void
    GenerateNextKey(void* u)
    {
      AsyncPathKeyExchangeContext< User >* ctx =
          static_cast< AsyncPathKeyExchangeContext< User >* >(u);

      // current hop
      auto& hop   = ctx->path->hops[ctx->idx];
      auto& frame = ctx->LRCM->frames[ctx->idx];
      // generate key
      ctx->crypto->encryption_keygen(hop.commkey);
      hop.nonce.Randomize();
      // do key exchange
      if(!ctx->crypto->dh_client(hop.shared, hop.router.enckey, hop.commkey,
                                 hop.nonce))
      {
        llarp::Error("Failed to generate shared key for path build");
        abort();
        return;
      }

      ++ctx->idx;

      bool isFarthestHop = ctx->idx == ctx->path->hops.size() - 1;

      if(isFarthestHop)
      {
        hop.upstream = hop.router.pubkey;
      }
      else
      {
        hop.upstream = ctx->path->hops[ctx->idx].router.pubkey;
      }

      // build record
      LR_CommitRecord record;
      record.version     = LLARP_PROTO_VERSION;
      record.txid        = hop.txID;
      record.rxid        = hop.rxID;
      record.tunnelNonce = hop.nonce;
      record.nextHop     = hop.upstream;
      record.commkey     = llarp::seckey_topublic(hop.commkey);

      auto buf = frame.Buffer();
      buf->cur = buf->base + EncryptedFrame::OverheadSize;
      // encode record
      if(!record.BEncode(buf))
      {
        // failed to encode?
        llarp::Error("Failed to generate Commit Record");
        return;
      }
      // use ephameral keypair for frame
      SecretKey framekey;
      ctx->crypto->encryption_keygen(framekey);
      if(!frame.EncryptInPlace(framekey, hop.router.enckey, ctx->crypto))
      {
        llarp::Error("Failed to encrypt LRCR");
        return;
      }

      if(isFarthestHop)
      {
        // farthest hop
        llarp_logic_queue_job(ctx->logic, {ctx, &HandleDone});
      }
      else
      {
        // next hop
        llarp_threadpool_queue_job(ctx->worker, {ctx, &GenerateNextKey});
      }
    }

    AsyncPathKeyExchangeContext(llarp_crypto* c) : crypto(c)
    {
    }

    /// Generate all keys asynchronously and call hadler when done
    void
    AsyncGenerateKeys(Path* p, llarp_logic* l, llarp_threadpool* pool, User* u,
                      Handler func)
    {
      path   = p;
      logic  = l;
      user   = u;
      result = func;
      worker = pool;
      LRCM   = new LR_CommitMessage;

      for(size_t idx = 0; idx < MAXHOPS; ++idx)
      {
        LRCM->frames.emplace_back();
        LRCM->frames.back().Randomize();
      }
      llarp_threadpool_queue_job(pool, {this, &GenerateNextKey});
    }
  };

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
    llarp::Info("Generated LRCM to ", remote);
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
  job->router = job->context->router;
  if(job->selectHop == nullptr)
    job->selectHop = &llarp_nodedb_select_random_hop;
  llarp_logic_queue_job(job->router->logic,
                        {job, &llarp::pathbuilder_start_build});
}
}