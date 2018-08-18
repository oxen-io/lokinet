#include <llarp/nodedb.h>
#include <llarp/path.hpp>

#include <llarp/pathbuilder.hpp>
#include "buffer.hpp"
#include "router.hpp"

namespace llarp
{
  template < typename User >
  struct AsyncPathKeyExchangeContext
  {
    typedef llarp::path::Path Path_t;
    typedef llarp::path::PathSet PathSet_t;
    PathSet_t* pathset = nullptr;
    Path_t* path       = nullptr;
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
        llarp::LogError("Failed to generate shared key for path build");
        abort();
        return;
      }
      // generate nonceXOR valueself->hop->pathKey
      ctx->crypto->shorthash(hop.nonceXOR, llarp::Buffer(hop.shared));
      ++ctx->idx;

      bool isFarthestHop = ctx->idx == ctx->path->hops.size();

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
        llarp::LogError("Failed to generate Commit Record");
        return;
      }
      // use ephameral keypair for frame
      SecretKey framekey;
      ctx->crypto->encryption_keygen(framekey);
      if(!frame.EncryptInPlace(framekey, hop.router.enckey, ctx->crypto))
      {
        llarp::LogError("Failed to encrypt LRCR");
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
    AsyncGenerateKeys(Path_t* p, llarp_logic* l, llarp_threadpool* pool,
                      User* u, Handler func)
    {
      path   = p;
      logic  = l;
      user   = u;
      result = func;
      worker = pool;
      LRCM   = new LR_CommitMessage();

      for(size_t idx = 0; idx < MAXHOPS; ++idx)
      {
        LRCM->frames.emplace_back();
        LRCM->frames.back().Randomize();
      }
      llarp_threadpool_queue_job(pool, {this, &GenerateNextKey});
    }
  };

  void
  pathbuilder_generated_keys(
      AsyncPathKeyExchangeContext< llarp_pathbuild_job >* ctx)
  {
    auto remote = ctx->path->Upstream();
    auto router = ctx->user->router;
    if(!router->SendToOrQueue(remote, ctx->LRCM))
    {
      llarp::LogError("failed to send LRCM");
      return;
    }

    ctx->path->status       = llarp::path::ePathBuilding;
    ctx->path->buildStarted = llarp_time_now_ms();
    // persist session with router until this path is done
    router->PersistSessionUntil(remote, ctx->path->ExpireTime());
    // add own path
    router->paths.AddOwnPath(ctx->pathset, ctx->path);
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
      if(!job->selectHop(job->user, job->router->nodedb, prev, rc, idx))
      {
        /// TODO: handle this failure properly
        llarp::LogWarn("Failed to select hop ", idx);
        return;
      }
      prev = rc;
      ++idx;
    }

    // async generate keys
    AsyncPathKeyExchangeContext< llarp_pathbuild_job >* ctx =
        new AsyncPathKeyExchangeContext< llarp_pathbuild_job >(
            &job->router->crypto);
    ctx->pathset = job->context;
    auto path    = new llarp::path::Path(&job->hops);
    path->SetBuildResultHook(std::bind(&llarp::path::PathSet::HandlePathBuilt,
                                       ctx->pathset, std::placeholders::_1));
    ctx->AsyncGenerateKeys(path, job->router->logic, job->router->tp, job,
                           &pathbuilder_generated_keys);
  }
}  // namespace llarp

llarp_pathbuilder_context::llarp_pathbuilder_context(
    llarp_router* p_router, struct llarp_dht_context* p_dht, size_t pathNum,
    size_t hops)
    : llarp::path::PathSet(pathNum), router(p_router), dht(p_dht), numHops(hops)
{
  p_router->paths.AddPathBuilder(this);
  p_router->crypto.encryption_keygen(enckey);
}

llarp_pathbuilder_context::~llarp_pathbuilder_context()
{
  router->paths.RemovePathBuilder(this);
}

bool
llarp_pathbuilder_context::SelectHop(llarp_nodedb* db, llarp_rc* prev,
                                     llarp_rc* cur, size_t hop)
{
  if(hop == 0)
    return router->GetRandomConnectedRouter(cur);
  else
    llarp_nodedb_select_random_hop(db, prev, cur, hop);
  return true;
}

byte_t*
llarp_pathbuilder_context::GetTunnelEncryptionSecretKey()
{
  return enckey;
}

void
llarp_pathbuilder_context::BuildOne()
{
  llarp_pathbuild_job* job = new llarp_pathbuild_job;
  job->context             = this;
  job->selectHop           = &PathSet::SelectHopCallback;
  job->hops.numHops        = numHops;
  job->user                = this;
  job->pathBuildStarted    = [](llarp_pathbuild_job* j) { delete j; };
  llarp_pathbuilder_build_path(job);
}

void
llarp_pathbuilder_context::ManualRebuild(size_t num)
{
  while(num--)
    BuildOne();
}

struct llarp_pathbuilder_context*
llarp_pathbuilder_context_new(struct llarp_router* router,
                              struct llarp_dht_context* dht, size_t sz,
                              size_t hops)
{
  return new llarp_pathbuilder_context(router, dht, sz, hops);
}

void
llarp_pathbuilder_context_free(struct llarp_pathbuilder_context* ctx)
{
  delete ctx;
}

void
llarp_pathbuilder_build_path(struct llarp_pathbuild_job* job)
{
  if(!job->context)
  {
    llarp::LogError("failed to build path because no context is set in job");
    return;
  }
  if(job->selectHop == nullptr)
  {
    llarp::LogError("No callback provided for hop selection");
    return;
  }
  job->router = job->context->router;
  llarp_logic_queue_job(job->router->logic,
                        {job, &llarp::pathbuilder_start_build});
}
