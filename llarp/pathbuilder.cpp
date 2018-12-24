#include <buffer.hpp>
#include <nodedb.hpp>
#include <path.hpp>
#include <pathbuilder.hpp>
#include <router.hpp>

#include <functional>

namespace llarp
{
  template < typename User >
  struct AsyncPathKeyExchangeContext
  {
    typedef llarp::path::Path Path_t;
    typedef llarp::path::Builder PathSet_t;
    PathSet_t* pathset = nullptr;
    Path_t* path       = nullptr;
    typedef std::function< void(AsyncPathKeyExchangeContext< User >*) > Handler;
    User* user = nullptr;

    Handler result;
    size_t idx               = 0;
    llarp::Router* router    = nullptr;
    llarp_threadpool* worker = nullptr;
    llarp::Logic* logic      = nullptr;
    llarp::Crypto* crypto    = nullptr;
    LR_CommitMessage LRCM;

    static void
    HandleDone(void* u)
    {
      AsyncPathKeyExchangeContext< User >* ctx =
          static_cast< AsyncPathKeyExchangeContext< User >* >(u);
      ctx->result(ctx);
      delete ctx;
    }

    static void
    GenerateNextKey(void* u)
    {
      AsyncPathKeyExchangeContext< User >* ctx =
          static_cast< AsyncPathKeyExchangeContext< User >* >(u);

      // current hop
      auto& hop   = ctx->path->hops[ctx->idx];
      auto& frame = ctx->LRCM.frames[ctx->idx];
      // generate key
      ctx->crypto->encryption_keygen(hop.commkey);
      hop.nonce.Randomize();
      // do key exchange
      if(!ctx->crypto->dh_client(hop.shared, hop.rc.enckey, hop.commkey,
                                 hop.nonce))
      {
        llarp::LogError("Failed to generate shared key for path build");
        delete ctx;
        return;
      }
      // generate nonceXOR valueself->hop->pathKey
      ctx->crypto->shorthash(hop.nonceXOR, llarp::Buffer(hop.shared));
      ++ctx->idx;

      bool isFarthestHop = ctx->idx == ctx->path->hops.size();

      if(isFarthestHop)
      {
        hop.upstream = hop.rc.pubkey.data();
      }
      else
      {
        hop.upstream = ctx->path->hops[ctx->idx].rc.pubkey.data();
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
      buf->cur = buf->base + EncryptedFrameOverheadSize;
      // encode record
      if(!record.BEncode(buf))
      {
        // failed to encode?
        llarp::LogError("Failed to generate Commit Record");
        llarp::DumpBuffer(*buf);
        delete ctx;
        return;
      }
      // use ephameral keypair for frame
      SecretKey framekey;
      ctx->crypto->encryption_keygen(framekey);
      if(!frame.EncryptInPlace(framekey, hop.rc.enckey, ctx->crypto))
      {
        llarp::LogError("Failed to encrypt LRCR");
        delete ctx;
        return;
      }

      if(isFarthestHop)
      {
        // farthest hop
        ctx->logic->queue_job({ctx, &HandleDone});
      }
      else
      {
        // next hop
        llarp_threadpool_queue_job(ctx->worker, {ctx, &GenerateNextKey});
      }
    }

    AsyncPathKeyExchangeContext(llarp::Crypto* c) : crypto(c)
    {
    }

    /// Generate all keys asynchronously and call hadler when done
    void
    AsyncGenerateKeys(Path_t* p, llarp::Logic* l, llarp_threadpool* pool,
                      User* u, Handler func)
    {
      path   = p;
      logic  = l;
      user   = u;
      result = func;
      worker = pool;

      for(size_t idx = 0; idx < MAXHOPS; ++idx)
      {
        LRCM.frames[idx].Randomize();
      }
      llarp_threadpool_queue_job(pool, {this, &GenerateNextKey});
    }
  };

  static void
  PathBuilderKeysGenerated(AsyncPathKeyExchangeContext< path::Builder >* ctx)
  {
    if(ctx->pathset->CanBuildPaths())
    {
      RouterID remote         = ctx->path->Upstream();
      const ILinkMessage* msg = &ctx->LRCM;
      if(ctx->router->SendToOrQueue(remote, msg))
      {
        // persist session with router until this path is done
        ctx->router->PersistSessionUntil(remote, ctx->path->ExpireTime());
        // add own path
        ctx->router->paths.AddOwnPath(ctx->pathset, ctx->path);
      }
      else
        llarp::LogError("failed to send LRCM to ", remote);
    }
    // decrement keygen counter
    ctx->pathset->keygens--;
  }

  namespace path
  {
    Builder::Builder(llarp::Router* p_router, struct llarp_dht_context* p_dht,
                     size_t pathNum, size_t hops)
        : llarp::path::PathSet(pathNum)
        , router(p_router)
        , dht(p_dht)
        , numHops(hops)
    {
      p_router->paths.AddPathBuilder(this);
      p_router->crypto.encryption_keygen(enckey);
      _run.store(true);
      keygens.store(0);
    }

    Builder::~Builder()
    {
      router->paths.RemovePathBuilder(this);
    }

    bool
    Builder::SelectHop(llarp_nodedb* db, const RouterContact& prev,
                       RouterContact& cur, size_t hop, PathRole roles)
    {
      (void)roles;
      if(hop == 0)
        return router->NumberOfConnectedRouters()
            && router->GetRandomConnectedRouter(cur);

      size_t tries = 5;
      do
      {
        --tries;
        if(db->select_random_hop(prev, cur, hop))
          return true;
      } while(router->routerProfiling.IsBad(cur.pubkey) && tries > 0);
      return false;
    }

    bool
    Builder::Stop()
    {
      _run.store(false);
      return true;
    }

    bool
    Builder::ShouldRemove() const
    {
      if(CanBuildPaths())
        return false;
      return keygens.load() > 0;
    }

    const byte_t*
    Builder::GetTunnelEncryptionSecretKey() const
    {
      return enckey;
    }

    bool
    Builder::ShouldBuildMore(llarp_time_t now) const
    {
      return llarp::path::PathSet::ShouldBuildMore(now) && now > lastBuild
          && now - lastBuild > buildIntervalLimit;
    }

    void
    Builder::BuildOne(PathRole roles)
    {
      std::vector< RouterContact > hops;
      if(SelectHops(router->nodedb, hops, roles))
        Build(hops, roles);
    }

    bool
    Builder::SelectHops(llarp_nodedb* nodedb,
                        std::vector< RouterContact >& hops, PathRole roles)
    {
      hops.resize(numHops);
      size_t idx = 0;
      while(idx < numHops)
      {
        if(idx == 0)
        {
          if(!SelectHop(nodedb, hops[0], hops[0], 0, roles))
          {
            llarp::LogError("failed to select first hop");
            return false;
          }
        }
        else
        {
          if(!SelectHop(nodedb, hops[idx - 1], hops[idx], idx, roles))
          {
            /// TODO: handle this failure properly
            llarp::LogWarn("Failed to select hop ", idx);
            return false;
          }
        }
        ++idx;
      }
      return true;
    }

    llarp_time_t
    Builder::Now() const
    {
      return router->Now();
    }

    void
    Builder::Build(const std::vector< RouterContact >& hops, PathRole roles)
    {
      if(!_run)
        return;
      lastBuild = Now();
      // async generate keys
      AsyncPathKeyExchangeContext< Builder >* ctx =
          new AsyncPathKeyExchangeContext< Builder >(&router->crypto);
      ctx->router  = router;
      ctx->pathset = this;
      auto path    = new llarp::path::Path(hops, this, roles);
      path->SetBuildResultHook(std::bind(&llarp::path::Builder::HandlePathBuilt,
                                         this, std::placeholders::_1));
      ++keygens;
      ctx->AsyncGenerateKeys(path, router->logic, router->tp, this,
                             &PathBuilderKeysGenerated);
    }

    void
    Builder::HandlePathBuilt(Path* p)
    {
      buildIntervalLimit = MIN_PATH_BUILD_INTERVAL;
      PathSet::HandlePathBuilt(p);
    }

    void
    Builder::HandlePathBuildTimeout(Path* p)
    {
      // linear backoff
      buildIntervalLimit += 1000;
      PathSet::HandlePathBuildTimeout(p);
    }

    void
    Builder::ManualRebuild(size_t num, PathRole roles)
    {
      llarp::LogDebug("manual rebuild ", num);
      while(num--)
        BuildOne(roles);
    }

  }  // namespace path
}  // namespace llarp
