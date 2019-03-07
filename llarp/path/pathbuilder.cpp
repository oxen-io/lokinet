#include <path/pathbuilder.hpp>

#include <messages/relay_commit.hpp>
#include <nodedb.hpp>
#include <path/path.hpp>
#include <profiling.hpp>
#include <router/abstractrouter.hpp>
#include <util/buffer.hpp>
#include <util/logic.hpp>

#include <functional>

namespace llarp
{
  template < typename User >
  struct AsyncPathKeyExchangeContext
  {
    typedef path::Path Path_t;
    typedef path::Builder PathSet_t;
    PathSet_t* pathset = nullptr;
    Path_t* path       = nullptr;
    typedef std::function< void(AsyncPathKeyExchangeContext< User >*) > Handler;
    User* user = nullptr;

    Handler result;
    size_t idx               = 0;
    AbstractRouter* router   = nullptr;
    llarp_threadpool* worker = nullptr;
    Logic* logic             = nullptr;
    Crypto* crypto           = nullptr;
    LR_CommitMessage LRCM;

    ~AsyncPathKeyExchangeContext()
    {
      if(path)
        delete path;
    }

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
        LogError("Failed to generate shared key for path build");
        delete ctx;
        return;
      }
      // generate nonceXOR valueself->hop->pathKey
      ctx->crypto->shorthash(hop.nonceXOR, llarp_buffer_t(hop.shared));
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
      record.commkey     = seckey_topublic(hop.commkey);

      llarp_buffer_t buf(frame.data(), frame.size());
      buf.cur = buf.base + EncryptedFrameOverheadSize;
      // encode record
      if(!record.BEncode(&buf))
      {
        // failed to encode?
        LogError("Failed to generate Commit Record");
        DumpBuffer(buf);
        delete ctx;
        return;
      }
      frame.Resize(buf.cur - buf.base);
      // use ephemeral keypair for frame
      SecretKey framekey;
      ctx->crypto->encryption_keygen(framekey);
      if(!frame.EncryptInPlace(framekey, hop.rc.enckey, ctx->crypto))
      {
        LogError("Failed to encrypt LRCR");
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

    AsyncPathKeyExchangeContext(Crypto* c) : crypto(c)
    {
    }

    /// Generate all keys asynchronously and call handler when done
    void
    AsyncGenerateKeys(Path_t* p, Logic* l, llarp_threadpool* pool, User* u,
                      Handler func)
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
    if(!ctx->pathset->IsStopped())
    {
      RouterID remote         = ctx->path->Upstream();
      const ILinkMessage* msg = &ctx->LRCM;
      if(ctx->router->SendToOrQueue(remote, msg))
      {
        // persist session with router until this path is done
        ctx->router->PersistSessionUntil(remote, ctx->path->ExpireTime());
        // add own path
        ctx->router->pathContext().AddOwnPath(ctx->pathset, ctx->path);
        ctx->path = nullptr;
      }
      else
        LogError("failed to send LRCM to ", remote);
    }
    // decrement keygen counter
    ctx->pathset->keygens--;
  }

  namespace path
  {
    Builder::Builder(AbstractRouter* p_router, struct llarp_dht_context* p_dht,
                     size_t pathNum, size_t hops)
        : path::PathSet(pathNum), router(p_router), dht(p_dht), numHops(hops)
    {
      p_router->pathContext().AddPathBuilder(this);
      p_router->crypto()->encryption_keygen(enckey);
      _run.store(true);
      keygens.store(0);
    }

    Builder::~Builder()
    {
      router->pathContext().RemovePathBuilder(this);
    }

    util::StatusObject
    Builder::ExtractStatus() const
    {
      util::StatusObject obj{{"keygens", uint64_t(keygens.load())},
                             {"numHops", uint64_t(numHops)},
                             {"numPaths", uint64_t(m_NumPaths)}};
      std::vector< util::StatusObject > pathObjs;
      std::transform(m_Paths.begin(), m_Paths.end(),
                     std::back_inserter(pathObjs),
                     [](const auto& item) -> util::StatusObject {
                       return item.second->ExtractStatus();
                     });
      obj.Put("paths", pathObjs);
      return obj;
    }

    bool
    Builder::SelectHop(llarp_nodedb* db, const RouterContact& prev,
                       RouterContact& cur, size_t hop, PathRole roles)
    {
      (void)roles;
      if(hop == 0)
        return router->NumberOfConnectedRouters()
            && router->GetRandomConnectedRouter(cur);

      size_t tries = 10;
      do
      {
        --tries;
        if(db->select_random_hop(prev, cur, hop))
          return true;
      } while(router->routerProfiling().IsBad(cur.pubkey) && tries > 0);
      return false;
    }

    bool
    Builder::Stop()
    {
      _run.store(false);
      return true;
    }

    bool
    Builder::IsStopped() const
    {
      return !_run.load();
    }

    bool
    Builder::ShouldRemove() const
    {
      if(!IsStopped())
        return false;
      return keygens.load() > 0;
    }

    const SecretKey&
    Builder::GetTunnelEncryptionSecretKey() const
    {
      return enckey;
    }

    bool
    Builder::BuildCooldownHit(llarp_time_t now) const
    {
      return now < lastBuild || now - lastBuild < buildIntervalLimit;
    }

    bool
    Builder::ShouldBuildMore(llarp_time_t now) const
    {
      if(llarp::randint() % 3 >= 1)
        return PathSet::ShouldBuildMore(now) && !BuildCooldownHit(now);
      return false;
    }

    void
    Builder::BuildOne(PathRole roles)
    {
      std::vector< RouterContact > hops;
      if(SelectHops(router->nodedb(), hops, roles))
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
            LogError("failed to select first hop");
            return false;
          }
        }
        else
        {
          if(!SelectHop(nodedb, hops[idx - 1], hops[idx], idx, roles))
          {
            /// TODO: handle this failure properly
            LogWarn("Failed to select hop ", idx);
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
      if(IsStopped())
        return;
      lastBuild = Now();
      // async generate keys
      AsyncPathKeyExchangeContext< Builder >* ctx =
          new AsyncPathKeyExchangeContext< Builder >(router->crypto());
      ctx->router  = router;
      ctx->pathset = this;
      auto path    = new path::Path(hops, this, roles);
      path->SetBuildResultHook(std::bind(&path::Builder::HandlePathBuilt, this,
                                         std::placeholders::_1));
      ++keygens;
      ctx->AsyncGenerateKeys(path, router->logic(), router->threadpool(), this,
                             &PathBuilderKeysGenerated);
    }

    void
    Builder::HandlePathBuilt(Path* p)
    {
      buildIntervalLimit = MIN_PATH_BUILD_INTERVAL;
      router->routerProfiling().MarkPathSuccess(p);
      PathSet::HandlePathBuilt(p);
    }

    void
    Builder::HandlePathBuildTimeout(Path* p)
    {
      // linear backoff
      static constexpr llarp_time_t MaxBuildInterval = 30 * 1000;
      buildIntervalLimit =
          std::max(1000 + buildIntervalLimit, MaxBuildInterval);
      router->routerProfiling().MarkPathFail(p);
      PathSet::HandlePathBuildTimeout(p);
    }

    void
    Builder::ManualRebuild(size_t num, PathRole roles)
    {
      LogDebug("manual rebuild ", num);
      while(num--)
        BuildOne(roles);
    }

  }  // namespace path
}  // namespace llarp
