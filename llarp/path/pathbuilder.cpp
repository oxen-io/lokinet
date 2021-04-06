#include "pathbuilder.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/messages/relay_commit.hpp>
#include <llarp/nodedb.hpp>
#include "path_context.hpp"
#include <llarp/profiling.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/tooling/path_event.hpp>

#include <functional>

namespace llarp
{
  struct AsyncPathKeyExchangeContext : std::enable_shared_from_this<AsyncPathKeyExchangeContext>
  {
    using WorkFunc_t = std::function<void(void)>;
    using WorkerFunc_t = std::function<void(WorkFunc_t)>;
    using Path_t = path::Path_ptr;
    using PathSet_t = path::PathSet_ptr;
    PathSet_t pathset = nullptr;
    Path_t path = nullptr;
    using Handler = std::function<void(std::shared_ptr<AsyncPathKeyExchangeContext>)>;

    Handler result;
    size_t idx = 0;
    AbstractRouter* router = nullptr;
    WorkerFunc_t work;
    EventLoop_ptr loop;
    LR_CommitMessage LRCM;

    void
    GenerateNextKey()
    {
      // current hop
      auto& hop = path->hops[idx];
      auto& frame = LRCM.frames[idx];

      auto crypto = CryptoManager::instance();

      // generate key
      crypto->encryption_keygen(hop.commkey);
      hop.nonce.Randomize();
      // do key exchange
      if (!crypto->dh_client(hop.shared, hop.rc.enckey, hop.commkey, hop.nonce))
      {
        LogError(pathset->Name(), " Failed to generate shared key for path build");
        return;
      }
      // generate nonceXOR valueself->hop->pathKey
      crypto->shorthash(hop.nonceXOR, llarp_buffer_t(hop.shared));
      ++idx;

      bool isFarthestHop = idx == path->hops.size();

      LR_CommitRecord record;
      if (isFarthestHop)
      {
        hop.upstream = hop.rc.pubkey;
      }
      else
      {
        hop.upstream = path->hops[idx].rc.pubkey;
        record.nextRC = std::make_unique<RouterContact>(path->hops[idx].rc);
      }
      // build record
      record.lifetime = path::default_lifetime;
      record.version = LLARP_PROTO_VERSION;
      record.txid = hop.txID;
      record.rxid = hop.rxID;
      record.tunnelNonce = hop.nonce;
      record.nextHop = hop.upstream;
      record.commkey = seckey_topublic(hop.commkey);

      llarp_buffer_t buf(frame.data(), frame.size());
      buf.cur = buf.base + EncryptedFrameOverheadSize;
      // encode record
      if (!record.BEncode(&buf))
      {
        // failed to encode?
        LogError(pathset->Name(), " Failed to generate Commit Record");
        DumpBuffer(buf);
        return;
      }
      // use ephemeral keypair for frame
      SecretKey framekey;
      crypto->encryption_keygen(framekey);
      if (!frame.EncryptInPlace(framekey, hop.rc.enckey))
      {
        LogError(pathset->Name(), " Failed to encrypt LRCR");
        return;
      }

      if (isFarthestHop)
      {
        // farthest hop
        // TODO: encrypt junk frames because our public keys are not eligator
        loop->call([self = shared_from_this()] { self->result(self); });
      }
      else
      {
        // next hop
        work([self = shared_from_this()] { self->GenerateNextKey(); });
      }
    }

    /// Generate all keys asynchronously and call handler when done
    void
    AsyncGenerateKeys(Path_t p, EventLoop_ptr l, WorkerFunc_t worker, Handler func)
    {
      path = p;
      loop = std::move(l);
      result = func;
      work = worker;

      for (size_t i = 0; i < path::max_len; ++i)
      {
        LRCM.frames[i].Randomize();
      }
      work([self = shared_from_this()] { self->GenerateNextKey(); });
    }
  };

  static void
  PathBuilderKeysGenerated(std::shared_ptr<AsyncPathKeyExchangeContext> ctx)
  {
    if (!ctx->pathset->IsStopped())
    {
      ctx->router->NotifyRouterEvent<tooling::PathAttemptEvent>(ctx->router->pubkey(), ctx->path);

      const RouterID remote = ctx->path->Upstream();
      const ILinkMessage* msg = &ctx->LRCM;
      auto sentHandler = [ctx](auto status) {
        if (status == SendStatus::Success)
        {
          ctx->router->pathContext().AddOwnPath(ctx->pathset, ctx->path);
          ctx->pathset->PathBuildStarted(std::move(ctx->path));
        }
        else
        {
          LogError(ctx->pathset->Name(), " failed to send LRCM to ", ctx->path->Upstream());
          ctx->path->EnterState(path::ePathFailed, ctx->router->Now());
        }
        ctx->path = nullptr;
        ctx->pathset = nullptr;
      };
      if (ctx->router->SendToOrQueue(remote, msg, sentHandler))
      {
        // persist session with router until this path is done
        if (ctx->path)
          ctx->router->PersistSessionUntil(remote, ctx->path->ExpireTime());
      }
      else
      {
        LogError(ctx->pathset->Name(), " failed to queue LRCM to ", remote);
        sentHandler(SendStatus::NoLink);
      }
    }
  }

  namespace path
  {
    Builder::Builder(AbstractRouter* p_router, size_t pathNum, size_t hops)
        : path::PathSet{pathNum}
        , m_EdgeLimiter{MIN_PATH_BUILD_INTERVAL}
        , _run{true}
        , m_router{p_router}
        , numHops{hops}
    {
      CryptoManager::instance()->encryption_keygen(enckey);
    }

    void
    Builder::ResetInternalState()
    {
      buildIntervalLimit = PATH_BUILD_RATE;
      lastBuild = 0s;
    }

    void Builder::Tick(llarp_time_t)
    {
      const auto now = llarp::time_now_ms();
      m_EdgeLimiter.Decay(now);
      ExpirePaths(now, m_router);
      if (ShouldBuildMore(now))
        BuildOne();
      TickPaths(m_router);
      if (m_BuildStats.attempts > 50)
      {
        if (m_BuildStats.SuccessRatio() <= BuildStats::MinGoodRatio && now - m_LastWarn > 5s)
        {
          LogWarn(Name(), " has a low path build success. ", m_BuildStats);
          m_LastWarn = now;
        }
      }
    }

    util::StatusObject
    Builder::ExtractStatus() const
    {
      util::StatusObject obj{
          {"buildStats", m_BuildStats.ExtractStatus()},
          {"numHops", uint64_t(numHops)},
          {"numPaths", uint64_t(numDesiredPaths)}};
      std::transform(
          m_Paths.begin(),
          m_Paths.end(),
          std::back_inserter(obj["paths"]),
          [](const auto& item) -> util::StatusObject { return item.second->ExtractStatus(); });
      return obj;
    }

    std::optional<RouterContact>
    Builder::SelectFirstHop(const std::set<RouterID>& exclude) const
    {
      std::optional<RouterContact> found = std::nullopt;
      m_router->ForEachPeer(
          [&](const ILinkSession* s, bool isOutbound) {
            if (s && s->IsEstablished() && isOutbound && not found.has_value())
            {
              const RouterContact rc = s->GetRemoteRC();
#ifndef TESTNET
              if (m_router->IsBootstrapNode(rc.pubkey))
                return;
#endif
              if (exclude.count(rc.pubkey))
                return;

              if (m_EdgeLimiter.Contains(rc.pubkey))
                return;

              found = rc;
            }
          },
          true);
      return found;
    }

    std::optional<std::vector<RouterContact>>
    Builder::GetHopsForBuild()
    {
      auto filter = [r = m_router](const auto& rc) -> bool {
        return not r->routerProfiling().IsBadForPath(rc.pubkey);
      };
      if (const auto maybe = m_router->nodedb()->GetRandom(filter))
      {
        return GetHopsAlignedToForBuild(maybe->pubkey);
      }
      return std::nullopt;
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
    Builder::BuildCooldownHit(RouterID edge) const
    {
      return m_EdgeLimiter.Contains(edge);
    }

    bool
    Builder::BuildCooldownHit(llarp_time_t now) const
    {
      return now < lastBuild + buildIntervalLimit;
    }

    bool
    Builder::ShouldBuildMore(llarp_time_t now) const
    {
      if (IsStopped())
        return false;
      if (BuildCooldownHit(now))
        return false;
      return PathSet::ShouldBuildMore(now);
    }

    void
    Builder::BuildOne(PathRole roles)
    {
      if (const auto maybe = GetHopsForBuild(); maybe.has_value())
        Build(*maybe, roles);
    }

    bool Builder::UrgentBuild(llarp_time_t) const
    {
      return buildIntervalLimit > MIN_PATH_BUILD_INTERVAL * 4;
    }

    std::optional<std::vector<RouterContact>>
    Builder::GetHopsAlignedToForBuild(RouterID endpoint, const std::set<RouterID>& exclude)
    {
      const auto pathConfig = m_router->GetConfig()->paths;

      std::vector<RouterContact> hops;
      {
        const auto maybe = SelectFirstHop(exclude);
        if (not maybe.has_value())
          return std::nullopt;
        hops.emplace_back(*maybe);
      };

      RouterContact endpointRC;
      if (const auto maybe = m_router->nodedb()->Get(endpoint))
      {
        endpointRC = *maybe;
      }
      else
        return std::nullopt;

      for (size_t idx = hops.size(); idx < numHops; ++idx)
      {
        if (idx + 1 == numHops)
        {
          hops.emplace_back(endpointRC);
        }
        else
        {
          auto filter =
              [&hops, r = m_router, endpointRC, pathConfig, exclude](const auto& rc) -> bool {
            if (exclude.count(rc.pubkey))
              return false;

            std::set<RouterContact> hopsSet;
            hopsSet.insert(endpointRC);
            hopsSet.insert(hops.begin(), hops.end());

            if (r->routerProfiling().IsBadForPath(rc.pubkey))
              return false;
            for (const auto& hop : hopsSet)
            {
              if (hop.pubkey == rc.pubkey)
                return false;
            }

            hopsSet.insert(rc);
            if (not pathConfig.Acceptable(hopsSet))
              return false;

            return rc.pubkey != endpointRC.pubkey;
          };

          if (const auto maybe = m_router->nodedb()->GetRandom(filter))
            hops.emplace_back(*maybe);
          else
            return std::nullopt;
        }
      }
      return hops;
    }

    bool
    Builder::BuildOneAlignedTo(const RouterID remote)
    {
      if (const auto maybe = GetHopsAlignedToForBuild(remote); maybe.has_value())
      {
        LogInfo(Name(), " building path to ", remote);
        Build(*maybe);
        return true;
      }
      return false;
    }

    llarp_time_t
    Builder::Now() const
    {
      return m_router->Now();
    }

    void
    Builder::Build(std::vector<RouterContact> hops, PathRole roles)
    {
      if (IsStopped())
        return;

      const RouterID edge{hops[0].pubkey};
      if (not m_EdgeLimiter.Insert(edge))
      {
        LogWarn(Name(), " building too fast to edge router ", edge);
        return;
      }

      lastBuild = Now();
      // async generate keys
      auto ctx = std::make_shared<AsyncPathKeyExchangeContext>();
      ctx->router = m_router;
      auto self = GetSelf();
      ctx->pathset = self;
      std::string path_shortName = "[path " + m_router->ShortName() + "-";
      path_shortName = path_shortName + std::to_string(m_router->NextPathBuildNumber()) + "]";
      auto path = std::make_shared<path::Path>(hops, self.get(), roles, std::move(path_shortName));
      LogInfo(Name(), " build ", path->ShortName(), ": ", path->HopsString());

      path->SetBuildResultHook([self](Path_ptr p) { self->HandlePathBuilt(p); });
      ctx->AsyncGenerateKeys(
          path,
          m_router->loop(),
          [r = m_router](auto func) { r->QueueWork(std::move(func)); },
          &PathBuilderKeysGenerated);
    }

    void
    Builder::HandlePathBuilt(Path_ptr p)
    {
      buildIntervalLimit = PATH_BUILD_RATE;
      m_router->routerProfiling().MarkPathSuccess(p.get());

      LogInfo(p->Name(), " built latency=", p->intro.latency);
      m_BuildStats.success++;
    }

    void
    Builder::HandlePathBuildFailedAt(Path_ptr p, RouterID edge)
    {
      PathSet::HandlePathBuildFailedAt(p, edge);
      DoPathBuildBackoff();
      /// add it to the edge limter even if it's not an edge for simplicity
      m_EdgeLimiter.Insert(edge);
    }

    void
    Builder::DoPathBuildBackoff()
    {
      static constexpr std::chrono::milliseconds MaxBuildInterval = 30s;
      // linear backoff
      buildIntervalLimit = std::min(PATH_BUILD_RATE + buildIntervalLimit, MaxBuildInterval);
      LogWarn(Name(), " build interval is now ", buildIntervalLimit);
    }

    void
    Builder::HandlePathBuildTimeout(Path_ptr p)
    {
      m_router->routerProfiling().MarkPathTimeout(p.get());
      PathSet::HandlePathBuildTimeout(p);
      DoPathBuildBackoff();
    }

    void
    Builder::ManualRebuild(size_t num, PathRole roles)
    {
      LogDebug(Name(), " manual rebuild ", num);
      while (num--)
        BuildOne(roles);
    }

  }  // namespace path
}  // namespace llarp
