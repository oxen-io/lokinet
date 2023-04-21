#include "pathbuilder.hpp"
#include "llarp/router_id.hpp"
#include "oxen/log.hpp"
#include "path_context.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/messages/relay_commit.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/profiling.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/router/i_rc_lookup_handler.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/tooling/path_event.hpp>
#include <llarp/link/link_manager.hpp>

#include <functional>
#include <memory>
#include <optional>

namespace llarp::path
{
  namespace
  {
    auto log_path = log::Cat("path");

    /// generate ephemeral keys for a dummy hop.
    path::PathHopConfig
    make_decoy_hop(Crypto* crypto)
    {
      path::PathHopConfig hop{};
      SecretKey sk{};
      crypto->encryption_keygen(sk);
      hop.rc.enckey = sk.toPublic();
      return hop;
    }

    /// does path key exchange in a worker thread and informs the logic thread on succsess.
    struct async_gen_path_keys
    {
      AbstractRouter& router;
      path::PathSet_ptr pathset;
      path::Path_ptr path_ptr;
      LR_CommitMessage LRCM;

      void
      operator()()
      {
        size_t idx{};
        const auto numhops = path_ptr->hops.size();
        auto crypto = CryptoManager::instance();

        // a hop config we use for dummy records
        path::PathHopConfig dummy{make_decoy_hop(crypto)};

        // generate records for all 8 slots
        for (auto& frame : LRCM.frames)
        {
          // start with random data in our frame.
          frame.Randomize();

          // get the hop config for the now.
          auto& hop = idx < numhops ? path_ptr->hops[idx] : dummy;

          // generate ephemeral encryptition keypair for this hop.
          crypto->encryption_keygen(hop.commkey);
          /// generate nounce
          hop.nonce.Randomize();
          // do key exchange
          if (!crypto->dh_client(hop.shared, hop.rc.enckey, hop.commkey, hop.nonce))
          {
            LogError(pathset->Name(), " Failed to generate shared key for path build");
            return;
          }
          // generate nonceXOR
          crypto->shorthash(hop.nonceXOR, llarp_buffer_t(hop.shared));

          LR_CommitRecord record{};

          if (idx == (numhops - 1))
          {
            // last real frame. points to ourself.
            hop.upstream = hop.rc.pubkey;
          }
          else if (idx < numhops)
          {
            // real frame. points to next hop.
            hop.upstream = path_ptr->hops[idx + 1].rc.pubkey;
          }
          else
          {}  // dummy frame. we dont set the upstream value as it doesnt matter.

          // increment the which hop we are on counter.
          idx++;

          // build the record
          record.lifetime = path::default_lifetime;
          record.version = llarp::constants::proto_version;
          record.txid = hop.txID;
          record.rxid = hop.rxID;
          record.tunnelNonce = hop.nonce;
          record.nextHop = hop.upstream;
          record.commkey = hop.commkey.toPublic();

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
        }
        inform();
      }

      void
      inform()
      {
        // this lambda called in the logic thread handles the successful completion of keygen.
        auto callback = [router = &router,
                         path_ptr = std::move(path_ptr),
                         pathset = std::move(pathset),
                         lrcm = std::move(LRCM)]() {
          if (pathset->IsStopped())
            return;

          router->NotifyRouterEvent<tooling::PathAttemptEvent>(router->pubkey(), path_ptr);

          router->pathContext().AddOwnPath(pathset, path_ptr);
          pathset->PathBuildStarted(path_ptr);

          const RouterID remote = path_ptr->upstream();
          auto sentHandler = [router = router, path_ptr = path_ptr](auto status) {
            if (status != SendStatus::Success)
            {
              path_ptr->EnterState(path::ePathFailed, router->Now());
            }
          };
          if (router->SendToOrQueue(remote, lrcm, sentHandler))
          {
            // persist session with router until this path is done
            router->PersistSessionUntil(remote, path_ptr->ExpireTime());
          }
          else
          {
            LogError(pathset->Name(), " failed to queue LRCM to ", remote);
            sentHandler(SendStatus::NoLink);
          }
        };

        router.loop()->call(std::move(callback));
      }
    };

  }  // namespace

  bool
  BuildLimiter::Attempt(const RouterID& router)
  {
    return m_EdgeLimiter.Insert(router);
  }

  void
  BuildLimiter::Decay(llarp_time_t now)
  {
    m_EdgeLimiter.Decay(now);
  }

  RouterID
  Builder::our_router_id() const
  {
    return m_router->pubkey();
  }

  bool
  BuildLimiter::Limited(const RouterID& router) const
  {
    return m_EdgeLimiter.Contains(router);
  }

  Builder::Builder(AbstractRouter* p_router, size_t pathNum, size_t hops)
      : path::PathSet{pathNum}, _run{true}, m_router{p_router}, numHops{hops}
  {
    CryptoManager::instance()->encryption_keygen(enckey);
  }

  void
  Builder::ResetInternalState()
  {
    buildIntervalLimit = PATH_BUILD_RATE;
    lastBuild = 0s;
  }

  void
  Builder::Tick(llarp_time_t now)
  {
    LogTrace("tick");
    ExpirePaths(now, m_router);
    PathSet::Tick(now);
    now = llarp::time_now_ms();
    m_router->pathBuildLimiter().Decay(now);

    if (ShouldBuildMore(now))
      BuildOne();
    LogTrace("tick paths");
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
        {"numHops", uint64_t{numHops}},
        {"numPaths", uint64_t{numDesiredPaths}}};
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
          if (s && s->IsEstablished() && isOutbound && not found)
          {
            const RouterContact rc = s->GetRemoteRC();

            if (not m_router->rcLookupHandler().PathIsAllowed(rc.pubkey))
              return;

            if (exclude.count(rc.pubkey))
              return;

            if (BuildCooldownHit(rc.pubkey))
              return;

            if (m_router->routerProfiling().IsBadForPath(rc.pubkey))
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
      return not r->routerProfiling().IsBadForPath(rc.pubkey, 1);
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
    // tell all our paths that they are to be ignored
    const auto now = Now();
    for (auto& item : m_Paths)
    {
      item.second->EnterState(ePathIgnore, now);
    }
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
    return IsStopped() and NumInStatus(ePathEstablished) == 0;
  }

  const SecretKey&
  Builder::GetTunnelEncryptionSecretKey() const
  {
    return enckey;
  }

  bool
  Builder::BuildCooldownHit(RouterID edge) const
  {
    return m_router->pathBuildLimiter().Limited(edge);
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
    if (m_router->NumberOfConnectedRouters() == 0)
      return false;
    return PathSet::ShouldBuildMore(now);
  }

  void
  Builder::BuildOne(PathRole roles)
  {
    if (const auto maybe = GetHopsForBuild())
      Build(*maybe, roles);
  }

  bool
  Builder::UrgentBuild(llarp_time_t) const
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
      if (not maybe)
      {
        if (m_router->NumberOfConnectedRouters())
          log::warning(
              log_path,
              "cannot build path for '{}' becuase we do not have enough edge connections to the "
              "network.",
              Name());
        else
          log::warning(
              log_path,
              "cannot build path for '{}' because we are not collected to the network",
              Name());

        m_router->connect_to_network();
        return std::nullopt;
      }
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

          if (r->routerProfiling().IsBadForPath(rc.pubkey, 1))
            return false;
          for (const auto& hop : hopsSet)
          {
            if (hop.pubkey == rc.pubkey)
              return false;
          }

          hopsSet.insert(rc);
#ifndef TESTNET
          if (not pathConfig.Acceptable(hopsSet))
            return false;
#endif
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

    lastBuild = Now();
    const RouterID edge{hops[0].pubkey};
    if (not m_router->pathBuildLimiter().Attempt(edge))
    {
      LogWarn(Name(), " building too fast to edge router ", edge);
      return;
    }
    LogTrace("build one aligning to ", RouterID{hops.back().pubkey});

    std::string path_shortName =
        fmt::format("[path {}-{}]", m_router->ShortName(), m_router->NextPathBuildNumber());
    auto self = shared_from_this();

    auto path = std::make_shared<path::Path>(hops, self, roles, std::move(path_shortName));
    LogInfo(Name(), " build ", path->ShortName(), ": ", path->HopsString());

    path->SetBuildResultHook([self](Path_ptr p) { self->HandlePathBuilt(p); });

    // async generate keys
    m_router->QueueWork(async_gen_path_keys{*m_router, self, path, LR_CommitMessage{}});
  }

  void
  Builder::HandlePathBuilt(Path_ptr p)
  {
    buildIntervalLimit = PATH_BUILD_RATE;
    m_router->routerProfiling().MarkPathSuccess(p.get());

    LogInfo(p->Name(), " built latency=", ToString(p->intro.latency));
    m_BuildStats.success++;
  }

  void
  Builder::HandlePathBuildFailedAt(Path_ptr p, RouterID edge)
  {
    PathSet::HandlePathBuildFailedAt(p, edge);
    DoPathBuildBackoff();
  }

  void
  Builder::DoPathBuildBackoff()
  {
    static constexpr std::chrono::milliseconds MaxBuildInterval = 30s;
    // linear backoff
    buildIntervalLimit = std::min(PATH_BUILD_RATE + buildIntervalLimit, MaxBuildInterval);
    LogWarn(Name(), " build interval is now ", ToString(buildIntervalLimit));
  }

  void
  Builder::HandlePathBuildTimeout(Path_ptr p)
  {
    m_router->routerProfiling().MarkPathTimeout(p.get());
    PathSet::HandlePathBuildTimeout(p);
    DoPathBuildBackoff();
    for (const auto& hop : p->hops)
    {
      const RouterID router{hop.rc.pubkey};
      // look up router and see if it's still on the network
      m_router->loop()->call_soon([router, r = m_router]() {
        LogInfo("looking up ", router, " because of path build timeout");
        r->rcLookupHandler().GetRC(
            router,
            [r](const auto& router, const auto* rc, auto result) {
              if (result == RCRequestResult::Success && rc != nullptr)
              {
                LogInfo("refreshed rc for ", router);
                r->nodedb()->PutIfNewer(*rc);
              }
              else
              {
                // remove all connections to this router as it's probably not registered anymore
                LogWarn("removing router ", router, " because of path build timeout");
                r->linkManager().DeregisterPeer(router);
                r->nodedb()->Remove(router);
              }
            },
            true);
      });
    }
  }

  void
  Builder::ManualRebuild(size_t num, PathRole roles)
  {
    LogDebug(Name(), " manual rebuild ", num);
    while (num--)
      BuildOne(roles);
  }

}  // namespace llarp::path
