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
    typedef path::Path_ptr Path_t;
    typedef path::PathSet_ptr PathSet_t;
    PathSet_t pathset = nullptr;
    Path_t path       = nullptr;
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
        LogError(ctx->pathset->Name(),
                 " Failed to generate shared key for path build");
        delete ctx;
        return;
      }
      // generate nonceXOR valueself->hop->pathKey
      ctx->crypto->shorthash(hop.nonceXOR, llarp_buffer_t(hop.shared));
      ++ctx->idx;

      bool isFarthestHop = ctx->idx == ctx->path->hops.size();

      LR_CommitRecord record;
      if(isFarthestHop)
      {
        hop.upstream = hop.rc.pubkey;
      }
      else
      {
        hop.upstream = ctx->path->hops[ctx->idx].rc.pubkey;
        record.nextRC =
            std::make_unique< RouterContact >(ctx->path->hops[ctx->idx].rc);
      }
      // build record

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
        LogError(ctx->pathset->Name(), " Failed to generate Commit Record");
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
        LogError(ctx->pathset->Name(), " Failed to encrypt LRCR");
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
    AsyncGenerateKeys(Path_t p, Logic* l, llarp_threadpool* pool, User* u,
                      Handler func)
    {
      path   = p;
      logic  = l;
      user   = u;
      result = func;
      worker = pool;

      for(size_t i = 0; i < path::max_len; ++i)
      {
        LRCM.frames[i].Randomize();
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
      }
      else
        LogError(ctx->pathset->Name(), " failed to send LRCM to ", remote);
    }
  }

  namespace path
  {
    Builder::Builder(AbstractRouter* p_router, struct llarp_dht_context* p_dht,
                     size_t pathNum, size_t hops)
        : path::PathSet(pathNum), router(p_router), dht(p_dht), numHops(hops)
    {
      p_router->crypto()->encryption_keygen(enckey);
      _run.store(true);
    }

    Builder::~Builder()
    {
    }

    void
    Builder::ResetInternalState()
    {
      buildIntervalLimit = MIN_PATH_BUILD_INTERVAL;
      lastBuild          = 0;
    }

    void
    Builder::Tick(llarp_time_t now)
    {
      ExpirePaths(now);
      if(ShouldBuildMore(now))
        BuildOne();
      TickPaths(now, router);
    }

    util::StatusObject
    Builder::ExtractStatus() const
    {
      util::StatusObject obj{{"numHops", uint64_t(numHops)},
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
      size_t tries = 10;
      if(hop == 0)
      {
        if(router->NumberOfConnectedRouters() == 0)
        {
          // persist connection
          router->ConnectToRandomRouters(1);
          return false;
        }
        bool got = false;
        router->ForEachPeer(
            [&](const ILinkSession* s, bool isOutbound) {
              if(s && s->IsEstablished() && isOutbound && !got)
              {
                const RouterContact rc = s->GetRemoteRC();
                if(got || router->IsBootstrapNode(rc.pubkey))
                  return;
                cur = rc;
                got = true;
              }
            },
            true);
        return got;
      }
      std::set< RouterID > exclude = {prev.pubkey};
      do
      {
        cur.Clear();
        --tries;
        if(db->select_random_hop_excluding(cur, exclude))
        {
          if(!router->routerProfiling().IsBadForPath(cur.pubkey))
            return true;
          exclude.insert(cur.pubkey);
        }
      } while(tries > 0);
      return tries > 0;
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
      return IsStopped();
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
      if(IsStopped())
        return false;
      return PathSet::ShouldBuildMore(now) && !BuildCooldownHit(now);
    }

    void
    Builder::BuildOne(PathRole roles)
    {
      std::vector< RouterContact > hops;
      if(SelectHops(router->nodedb(), hops, roles))
        Build(hops, roles);
    }

    bool Builder::UrgentBuild(llarp_time_t) const
    {
      return buildIntervalLimit > MIN_PATH_BUILD_INTERVAL * 4;
    }

    bool
    Builder::BuildOneAlignedTo(const RouterID remote)
    {
      std::vector< RouterContact > hops;

      /// if we really need this path build it "dangerously"
      if(UrgentBuild(router->Now()))
      {
        const auto aligned =
            router->pathContext().FindOwnedPathsWithEndpoint(remote);
        /// pick the lowest latency path that aligns to remote
        /// note: peer exuastion is made worse happen here
        Path_ptr p;
        llarp_time_t min = std::numeric_limits< llarp_time_t >::max();
        for(const auto& path : aligned)
        {
          if(path->intro.latency < min && path->hops.size() == numHops)
          {
            p   = path;
            min = path->intro.latency;
          }
        }
        if(p)
        {
          for(const auto& hop : p->hops)
            hops.emplace_back(hop.rc);
        }
      }
      if(hops.size() == 0)
      {
        hops.resize(numHops);

        auto nodedb = router->nodedb();
        for(size_t hop = 0; hop < numHops; ++hop)
        {
          if(hop == 0)
          {
            if(!SelectHop(nodedb, hops[0], hops[0], 0, path::ePathRoleAny))
              return false;
          }
          else if(hop == numHops - 1)
          {
            // last hop
            if(!nodedb->Get(remote, hops[hop]))
            {
              router->LookupRouter(remote, nullptr);
              return false;
            }
          }
          // middle hop
          else
          {
            size_t tries = 5;
            do
            {
              nodedb->select_random_hop_excluding(
                  hops[hop], {hops[hop - 1].pubkey, remote});
              --tries;
            } while(router->routerProfiling().IsBadForPath(hops[hop].pubkey)
                    && tries > 0);
            return tries > 0;
          }
          return false;
        }
      }
      LogInfo(Name(), " building path to ", remote);
      Build(hops);
      return true;
    }

    bool
    Builder::SelectHops(llarp_nodedb* nodedb,
                        std::vector< RouterContact >& hops, PathRole roles)
    {
      hops.resize(numHops);
      size_t idx = 0;
      while(idx < numHops)
      {
        size_t tries = 4;
        if(idx == 0)
        {
          while(tries > 0 && !SelectHop(nodedb, hops[0], hops[0], 0, roles))
            --tries;
        }
        else
        {
          while(tries > 0
                && !SelectHop(nodedb, hops[idx - 1], hops[idx], idx, roles))
            --tries;
        }
        if(tries == 0)
        {
          LogWarn(Name(), " failed to select hop ", idx);
          return false;
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
      ctx->pathset = GetSelf();
      auto path    = std::make_shared< path::Path >(hops, this, roles);
      path->SetBuildResultHook(
          [this](Path_ptr p) { this->HandlePathBuilt(p); });
      ctx->AsyncGenerateKeys(path, router->logic(), router->threadpool(), this,
                             &PathBuilderKeysGenerated);
    }

    void
    Builder::HandlePathBuilt(Path_ptr p)
    {
      buildIntervalLimit = MIN_PATH_BUILD_INTERVAL;
      router->routerProfiling().MarkPathSuccess(p.get());
      LogInfo(p->Name(), " built latency=", p->intro.latency);
    }

    void
    Builder::HandlePathBuildTimeout(Path_ptr p)
    {
      // linear backoff
      static constexpr llarp_time_t MaxBuildInterval = 30 * 1000;
      buildIntervalLimit                             = std::min(
          MIN_PATH_BUILD_INTERVAL + buildIntervalLimit, MaxBuildInterval);
      router->routerProfiling().MarkPathFail(p.get());
      PathSet::HandlePathBuildTimeout(p);
      LogWarn(Name(), " build interval is now ", buildIntervalLimit);
    }

    void
    Builder::ManualRebuild(size_t num, PathRole roles)
    {
      LogDebug(Name(), " manual rebuild ", num);
      while(num--)
        BuildOne(roles);
    }

  }  // namespace path
}  // namespace llarp
