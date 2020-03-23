#include <path/pathbuilder.hpp>

#include <crypto/crypto.hpp>
#include <messages/relay_commit.hpp>
#include <nodedb.hpp>
#include <path/path_context.hpp>
#include <profiling.hpp>
#include <router/abstractrouter.hpp>
#include <util/buffer.hpp>
#include <util/thread/logic.hpp>

#include <functional>

namespace llarp
{
  struct AsyncPathKeyExchangeContext
      : std::enable_shared_from_this< AsyncPathKeyExchangeContext >
  {
    using Path_t      = path::Path_ptr;
    using PathSet_t   = path::PathSet_ptr;
    PathSet_t pathset = nullptr;
    Path_t path       = nullptr;
    using Handler =
        std::function< void(std::shared_ptr< AsyncPathKeyExchangeContext >) >;

    Handler result;
    size_t idx             = 0;
    AbstractRouter* router = nullptr;
    std::shared_ptr< thread::ThreadPool > worker;
    std::shared_ptr< Logic > logic;
    LR_CommitMessage LRCM;

    void
    GenerateNextKey()
    {
      // current hop
      auto& hop   = path->hops[idx];
      auto& frame = LRCM.frames[idx];

      auto crypto = CryptoManager::instance();

      // generate key
      crypto->encryption_keygen(hop.commkey);
      hop.nonce.Randomize();
      // do key exchange
      if(!crypto->dh_client(hop.shared, hop.rc.enckey, hop.commkey, hop.nonce))
      {
        LogError(pathset->Name(),
                 " Failed to generate shared key for path build");
        return;
      }
      // generate nonceXOR valueself->hop->pathKey
      crypto->shorthash(hop.nonceXOR, llarp_buffer_t(hop.shared));
      ++idx;

      bool isFarthestHop = idx == path->hops.size();

      LR_CommitRecord record;
      if(isFarthestHop)
      {
        hop.upstream = hop.rc.pubkey;
      }
      else
      {
        hop.upstream  = path->hops[idx].rc.pubkey;
        record.nextRC = std::make_unique< RouterContact >(path->hops[idx].rc);
      }
      // build record
      record.lifetime    = path::default_lifetime;
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
        LogError(pathset->Name(), " Failed to generate Commit Record");
        DumpBuffer(buf);
        return;
      }
      // use ephemeral keypair for frame
      SecretKey framekey;
      crypto->encryption_keygen(framekey);
      if(!frame.EncryptInPlace(framekey, hop.rc.enckey))
      {
        LogError(pathset->Name(), " Failed to encrypt LRCR");
        return;
      }

      if(isFarthestHop)
      {
        // farthest hop
        // TODO: encrypt junk frames because our public keys are not eligator
        LogicCall(logic, std::bind(result, shared_from_this()));
      }
      else
      {
        // next hop
        worker->addJob(std::bind(&AsyncPathKeyExchangeContext::GenerateNextKey,
                                 shared_from_this()));
      }
    }

    /// Generate all keys asynchronously and call handler when done
    void
    AsyncGenerateKeys(Path_t p, std::shared_ptr< Logic > l,
                      std::shared_ptr< thread::ThreadPool > pool, Handler func)
    {
      path   = p;
      logic  = l;
      result = func;
      worker = pool;

      for(size_t i = 0; i < path::max_len; ++i)
      {
        LRCM.frames[i].Randomize();
      }
      pool->addJob(std::bind(&AsyncPathKeyExchangeContext::GenerateNextKey,
                             shared_from_this()));
    }
  };

  static void
  PathBuilderKeysGenerated(std::shared_ptr< AsyncPathKeyExchangeContext > ctx)
  {
    if(!ctx->pathset->IsStopped())
    {
      const RouterID remote   = ctx->path->Upstream();
      const ILinkMessage* msg = &ctx->LRCM;
      auto sentHandler        = [ctx](auto status) {
        if(status == SendStatus::Success)
        {
          ctx->router->pathContext().AddOwnPath(ctx->pathset, ctx->path);
          ctx->pathset->PathBuildStarted(ctx->path);
        }
        else
        {
          LogError(ctx->pathset->Name(), " failed to send LRCM to ",
                   ctx->path->Upstream());
          ctx->pathset->HandlePathBuildFailed(ctx->path);
        }
      };
      if(ctx->router->SendToOrQueue(remote, msg, sentHandler))
      {
        // persist session with router until this path is done
        ctx->router->PersistSessionUntil(remote, ctx->path->ExpireTime());
      }
      else
        LogError(ctx->pathset->Name(), " failed to queue LRCM to ", remote);
    }
  }

  namespace path
  {
    Builder::Builder(AbstractRouter* p_router, size_t pathNum, size_t hops)
        : path::PathSet(pathNum), _run(true), m_router(p_router), numHops(hops)
    {
      CryptoManager::instance()->encryption_keygen(enckey);
    }

    void
    Builder::ResetInternalState()
    {
      buildIntervalLimit = MIN_PATH_BUILD_INTERVAL;
      lastBuild          = 0s;
    }

    void Builder::Tick(llarp_time_t)
    {
      const auto now = llarp::time_now_ms();
      ExpirePaths(now, m_router);
      if(ShouldBuildMore(now))
        BuildOne();
      TickPaths(m_router);
      if(m_BuildStats.attempts > 50)
      {
        if(m_BuildStats.SuccessRatio() <= BuildStats::MinGoodRatio
           && now - m_LastWarn > 5s)
        {
          LogWarn(Name(), " has a low path build success. ", m_BuildStats);
          m_LastWarn = now;
        }
      }
    }

    util::StatusObject
    Builder::ExtractStatus() const
    {
      util::StatusObject obj{{"buildStats", m_BuildStats.ExtractStatus()},
                             {"numHops", uint64_t(numHops)},
                             {"numPaths", uint64_t(numPaths)}};
      std::transform(m_Paths.begin(), m_Paths.end(),
                     std::back_inserter(obj["paths"]),
                     [](const auto& item) -> util::StatusObject {
                       return item.second->ExtractStatus();
                     });
      return obj;
    }

    bool
    Builder::SelectHop(llarp_nodedb* db, const std::set< RouterID >& exclude,
                       const std::vector< IPRange >& prevRanges,
                       RouterContact& cur, size_t hop, PathRole roles)
    {
      (void)roles;
      size_t tries = 10;
      if(hop == 0)
      {
        if(m_router->NumberOfConnectedRouters() == 0)
        {
          // persist connection
          m_router->ConnectToRandomRouters(1);
          return false;
        }
        bool got = false;
        m_router->ForEachPeer(
            [&](const ILinkSession* s, bool isOutbound) {
              if(s && s->IsEstablished() && isOutbound && !got)
              {
                const RouterContact rc = s->GetRemoteRC();
#ifdef TESTNET
                if(got || exclude.count(rc.pubkey))
#else
                if(got || exclude.count(rc.pubkey)
                   || m_router->IsBootstrapNode(rc.pubkey))
#endif
                  return;
                cur = rc;
                got = true;
              }
            },
            true);
        return got;
      }

      do
      {
        cur.Clear();
        --tries;
        if(db->select_random_hop_excluding_ranges(cur, prevRanges))
        {
          if(!m_router->routerProfiling().IsBadForPath(cur.pubkey))
            return true;
        }
      } while(tries > 0);

      return false;
    }

    bool
    Builder::Stop()
    {
      _run = false;
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
      return now < lastBuild + buildIntervalLimit;
    }

    bool
    Builder::ShouldBuildMore(llarp_time_t now) const
    {
      if(IsStopped())
        return false;
      if(BuildCooldownHit(now))
        return false;
      return PathSet::ShouldBuildMore(now);
    }

    void
    Builder::BuildOne(PathRole roles)
    {
      std::vector< RouterContact > hops(numHops);
      if(SelectHops(m_router->nodedb(), hops, roles))
        Build(hops, roles);
    }

    bool Builder::UrgentBuild(llarp_time_t) const
    {
      return buildIntervalLimit > MIN_PATH_BUILD_INTERVAL * 4;
    }

    bool
    Builder::DoUrgentBuildAlignedTo(const RouterID remote,
                                    std::vector< RouterContact >& hops)
    {
      const auto aligned =
          m_router->pathContext().FindOwnedPathsWithEndpoint(remote);
      /// pick the lowest latency path that aligns to remote
      /// note: peer exhaustion is made worse happen here
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
        {
          if(hop.rc.pubkey.IsZero())
            return false;
          hops.emplace_back(hop.rc);
        }
      }

      return true;
    }

    bool
    Builder::DoBuildAlignedTo(const RouterID remote,
                              std::vector< RouterContact >& hops)
    {
      std::set< RouterID > routers{remote};
      hops.resize(numHops);
      std::vector< IPRange > ranges;
      auto nodedb = m_router->nodedb();
      for(size_t idx = 0; idx < hops.size(); idx++)
      {
        hops[idx].Clear();
        if(idx == numHops - 1)
        {
          // last hop
          if(!nodedb->Get(remote, hops[idx]))
          {
            m_router->LookupRouter(remote, nullptr);
            return false;
          }
        }
        else
        {
          if(!SelectHop(nodedb, routers, ranges, hops[idx], idx,
                        path::ePathRoleAny))
          {
            return false;
          }
        }
        if(hops[idx].pubkey.IsZero())
          return false;
        routers.insert(hops[idx].pubkey);
        for(const auto& addrInfo : hops[idx].addrs)
          ranges.push_back(addrInfo.IPRangeV4(16));
      }

      return true;
    }

    bool
    Builder::BuildOneAlignedTo(const RouterID remote)
    {
      std::vector< RouterContact > hops;
      /// if we really need this path build it "dangerously"
      if(UrgentBuild(m_router->Now()))
      {
        if(!DoUrgentBuildAlignedTo(remote, hops))
        {
          return false;
        }
      }

      if(hops.empty())
      {
        if(!DoBuildAlignedTo(remote, hops))
        {
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
      std::set< RouterID > exclude;
      std::vector< IPRange > ranges;
      for(size_t idx = 0; idx < hops.size(); ++idx)
      {
        hops[idx].Clear();
        size_t tries = 32;
        while(tries > 0
              && !SelectHop(nodedb, exclude, ranges, hops[idx], idx, roles))
        {
          --tries;
        }
        if(tries == 0 || hops[idx].pubkey.IsZero())
        {
          LogWarn(Name(), " failed to select hop ", idx);
          return false;
        }
        exclude.emplace(hops[idx].pubkey);
        for(const auto& addrInfo : hops[idx].addrs)
        {
          const auto range = addrInfo.IPRangeV4(16);
          ranges.push_back(range);
        }
      }
      return true;
    }

    llarp_time_t
    Builder::Now() const
    {
      return m_router->Now();
    }

    void
    Builder::Build(const std::vector< RouterContact >& hops, PathRole roles)
    {
      if(IsStopped())
        return;
      lastBuild = Now();
      // async generate keys
      auto ctx     = std::make_shared< AsyncPathKeyExchangeContext >();
      ctx->router  = m_router;
      auto self    = GetSelf();
      ctx->pathset = self;
      std::string path_shortName = "[path " + m_router->ShortName() + "-";
      path_shortName             = path_shortName
          + std::to_string(m_router->NextPathBuildNumber()) + "]";
      auto path = std::make_shared< path::Path >(hops, self.get(), roles,
                                                 std::move(path_shortName));
      LogInfo(Name(), " build ", path->ShortName(), ": ", path->HopsString());
      path->SetBuildResultHook(
          [self](Path_ptr p) { self->HandlePathBuilt(p); });
      ctx->AsyncGenerateKeys(path, m_router->logic(), m_router->threadpool(),
                             &PathBuilderKeysGenerated);
    }

    void
    Builder::HandlePathBuilt(Path_ptr p)
    {
      buildIntervalLimit = MIN_PATH_BUILD_INTERVAL;
      m_router->routerProfiling().MarkPathSuccess(p.get());
      LogInfo(p->Name(), " built latency=", p->intro.latency);
      m_BuildStats.success++;
    }

    void
    Builder::HandlePathBuildFailed(Path_ptr p)
    {
      m_router->routerProfiling().MarkPathFail(p.get());
      PathSet::HandlePathBuildFailed(p);
      DoPathBuildBackoff();
    }

    void
    Builder::DoPathBuildBackoff()
    {
      static constexpr std::chrono::milliseconds MaxBuildInterval = 30s;
      // linear backoff
      buildIntervalLimit = std::min(
          MIN_PATH_BUILD_INTERVAL + buildIntervalLimit, MaxBuildInterval);
      LogWarn(Name(), " build interval is now ", buildIntervalLimit);
    }

    void
    Builder::HandlePathBuildTimeout(Path_ptr p)
    {
      m_router->routerProfiling().MarkPathFail(p.get());
      PathSet::HandlePathBuildTimeout(p);
      DoPathBuildBackoff();
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
