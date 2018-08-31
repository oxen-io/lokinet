#include <llarp/nodedb.hpp>
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
      if(!ctx->crypto->dh_client(hop.shared, hop.rc.enckey, hop.commkey,
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
        hop.upstream = hop.rc.pubkey;
      }
      else
      {
        hop.upstream = ctx->path->hops[ctx->idx].rc.pubkey;
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
      if(!frame.EncryptInPlace(framekey, hop.rc.enckey, ctx->crypto))
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
        LRCM->frames[idx].Randomize();
      }
      llarp_threadpool_queue_job(pool, {this, &GenerateNextKey});
    }
  };

  void
  pathbuilder_generated_keys(AsyncPathKeyExchangeContext< path::Builder >* ctx)
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
  }

  namespace path
  {
    Builder::Builder(llarp_router* p_router, struct llarp_dht_context* p_dht,
                     size_t pathNum, size_t hops)
        : llarp::path::PathSet(pathNum)
        , router(p_router)
        , dht(p_dht)
        , numHops(hops)
    {
      p_router->paths.AddPathBuilder(this);
      p_router->crypto.encryption_keygen(enckey);
    }

    Builder::~Builder()
    {
      router->paths.RemovePathBuilder(this);
    }

    bool
    Builder::SelectHop(llarp_nodedb* db, const RouterContact& prev,
                       RouterContact& cur, size_t hop)
    {
      if(hop == 0)
      {
        if(router->NumberOfConnectedRouters())
          return router->GetRandomConnectedRouter(cur);
        else
          return llarp_nodedb_select_random_hop(db, prev, cur, 0);
      }
      return llarp_nodedb_select_random_hop(db, prev, cur, hop);
    }

    const byte_t*
    Builder::GetTunnelEncryptionSecretKey() const
    {
      return enckey;
    }

    bool
    Builder::ShouldBuildMore() const
    {
      return llarp::path::PathSet::ShouldBuildMore()
          || router->NumberOfConnectedRouters() == 0;
    }

    void
    Builder::BuildOne()
    {
      // select hops
      std::vector< RouterContact > hops;
      for(size_t i = 0; i < numHops; ++i)
        hops.emplace_back();
      size_t idx = 0;
      while(idx < numHops)
      {
        if(idx == 0)
        {
          if(!SelectHop(router->nodedb, hops[0], hops[0], 0))
          {
            llarp::LogError("failed to select first hop");
            return;
          }
        }
        else
        {
          if(!SelectHop(router->nodedb, hops[idx - 1], hops[idx], idx))
          {
            /// TODO: handle this failure properly
            llarp::LogWarn("Failed to select hop ", idx);
            return;
          }
        }
        ++idx;
      }
      // async generate keys
      AsyncPathKeyExchangeContext< Builder >* ctx =
          new AsyncPathKeyExchangeContext< Builder >(&router->crypto);
      ctx->pathset = this;
      auto path    = new llarp::path::Path(hops);
      path->SetBuildResultHook(std::bind(&llarp::path::PathSet::HandlePathBuilt,
                                         ctx->pathset, std::placeholders::_1));
      ctx->AsyncGenerateKeys(path, router->logic, router->tp, this,
                             &pathbuilder_generated_keys);
    }

    void
    Builder::ManualRebuild(size_t num)
    {
      llarp::LogDebug("manual rebuild ", num);
      while(num--)
        BuildOne();
    }

  }  // namespace path
}  // namespace llarp
