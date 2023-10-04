#include "pathbuilder.hpp"
#include "path_context.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/messages/relay_commit.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/profiling.hpp>
#include <llarp/router/router.hpp>
#include <llarp/router/rc_lookup_handler.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/tooling/path_event.hpp>
#include <llarp/link/link_manager.hpp>

#include <functional>

namespace llarp
{
  namespace
  {
    auto log_path = log::Cat("path");
  }

  namespace path
  {
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

    bool
    BuildLimiter::Limited(const RouterID& router) const
    {
      return m_EdgeLimiter.Contains(router);
    }

    Builder::Builder(Router* p_router, size_t pathNum, size_t hops)
        : path::PathSet{pathNum}, _run{true}, router{p_router}, numHops{hops}
    {}

    /* - For each hop:
     * SetupHopKeys:
     *   - Generate Ed keypair for the hop. ("commkey")
     *   - Use that key and the hop's pubkey for DH key exchange (makes "hop.shared")
     *     - Note: this *was* using hop's "enckey" but we're getting rid of that
     *   - hop's "upstream" RouterID is next hop, or that hop's ID if it is terminal hop
     *   - hop's chacha nonce is hash of symmetric key (hop.shared) from DH
     *   - hop's "txID" and "rxID" are chosen before this step
     *     - txID is the path ID for messages coming *from* the client/path origin
     *     - rxID is the path ID for messages going *to* it.
     *
     * CreateHopInfoFrame:
     *   - bt-encode "hop info":
     *     - path lifetime
     *     - protocol version
     *     - txID
     *     - rxID
     *     - nonce
     *     - upstream hop RouterID
     *     - ephemeral public key (for DH)
     *   - generate *second* ephemeral Ed keypair... ("framekey") TODO: why?
     *   - generate DH symmetric key using "framekey" and hop's pubkey
     *   - generate nonce for second encryption
     *   - encrypt "hop info" using this symmetric key
     *   - bt-encode nonce, "framekey" pubkey, encrypted "hop info"
     *   - hash this bt-encoded string
     *   - bt-encode hash and the frame in a dict, serialize
     *
     *
     *  all of these "frames" go in a list, along with any needed dummy frames
     */

    void
    Builder::SetupHopKeys(path::PathHopConfig& hop, const RouterID& nextHop)
    {
      auto crypto = CryptoManager::instance();

      // generate key
      crypto->encryption_keygen(hop.commkey);

      hop.nonce.Randomize();
      // do key exchange
      if (!crypto->dh_client(hop.shared, hop.rc.pubkey, hop.commkey, hop.nonce))
      {
        LogError(Name(), " Failed to generate shared key for path build");
        throw std::runtime_error{"Failed to generate shared key for path build"};
      }
      // generate nonceXOR value self->hop->pathKey
      crypto->shorthash(hop.nonceXOR, llarp_buffer_t(hop.shared));

      hop.upstream = nextHop;
    }

    // FIXME: this is definitely not optimal using bt_dict instead of bt_dict_producer, among other
    // things
    std::string
    Builder::CreateHopInfoFrame(const path::PathHopConfig& hop)
    {
      auto crypto = CryptoManager::instance();

      oxenc::bt_dict hop_info_dict;
      hop_info_dict["lifetime"] = path::DEFAULT_LIFETIME.count();  // milliseconds
      hop_info_dict["txid"] = hop.txID;
      hop_info_dict["rxid"] = hop.rxID;
      hop_info_dict["nonce"] = hop.nonce;
      hop_info_dict["next"] = hop.upstream;
      hop_info_dict["commkey"] = hop.commkey.toPublic();  // pubkey of ephemeral Ed key for DH

      auto hop_info = oxenc::bt_serialize(hop_info_dict);
      SecretKey framekey;
      crypto->encryption_keygen(framekey);

      SharedSecret shared;
      TunnelNonce outer_nonce;
      outer_nonce.Randomize();
      // derive (outer) shared key
      if (!crypto->dh_client(shared, hop.rc.pubkey, framekey, outer_nonce))
      {
        llarp::LogError("DH failed during hop info encryption");
        throw std::runtime_error{"DH failed during hop info encryption"};
      }

      // encrypt hop_info (mutates in-place)
      if (!crypto->xchacha20(
              reinterpret_cast<uint8_t*>(hop_info.data()), hop_info.size(), shared, outer_nonce))
      {
        llarp::LogError("hop info encrypt failed");
        throw std::runtime_error{"hop info encrypt failed"};
      }
      oxenc::bt_dict hashed_dict;
      hashed_dict["encrypted"] = hop_info;
      hashed_dict["pubkey"] = framekey.toPublic();
      hashed_dict["nonce"] = nonce;
      auto hashed_data = oxenc::bt_serialize(hashed_dict);

      std::basic_string<uint8_t> hash{SHORTHASHSIZE, '\0'};
      if (!crypto->hmac(
              hash.data(),
              reinterpret_cast<uint8_t*>(hashed_data.data()),
              hashed_data.size(),
              shared))
      {
        llarp::LogError("Failed to generate HMAC for hop info");
        throw std::runtime_error{"Failed to generate HMAC for hop info"};
      }

      oxenc::bt_dict final_frame;
      final_frame["hash"] = hash;
      final_frame["frame"] = hashed_data;

      return oxenc::bt_serialize(final_frame);
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
      PathSet::Tick(now);
      now = llarp::time_now_ms();
      router->pathbuild_limiter().Decay(now);

      ExpirePaths(now, router);
      if (ShouldBuildMore(now))
        BuildOne();
      TickPaths(router);
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
      router->for_each_connection([&](link::Connection& conn) {
        const auto& rc = conn.remote_rc;
#ifndef TESTNET
        if (router->IsBootstrapNode(rc.pubkey))
          return;
#endif
        if (exclude.count(rc.pubkey))
          return;

        if (BuildCooldownHit(rc.pubkey))
          return;

        if (router->router_profiling().IsBadForPath(rc.pubkey))
          return;

        found = rc;
      });
      return found;
    }

    std::optional<std::vector<RouterContact>>
    Builder::GetHopsForBuild()
    {
      auto filter = [r = router](const auto& rc) -> bool {
        return not r->router_profiling().IsBadForPath(rc.pubkey, 1);
      };
      if (const auto maybe = router->node_db()->GetRandom(filter))
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

    bool
    Builder::BuildCooldownHit(RouterID edge) const
    {
      return router->pathbuild_limiter().Limited(edge);
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
      const auto pathConfig = router->config()->paths;

      std::vector<RouterContact> hops;
      {
        const auto maybe = SelectFirstHop(exclude);
        if (not maybe.has_value())
        {
          log::warning(log_path, "{} has no first hop candidate", Name());
          return std::nullopt;
        }
        hops.emplace_back(*maybe);
      };

      RouterContact endpointRC;
      if (const auto maybe = router->node_db()->Get(endpoint))
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
              [&hops, r = router, endpointRC, pathConfig, exclude](const auto& rc) -> bool {
            if (exclude.count(rc.pubkey))
              return false;

            std::set<RouterContact> hopsSet;
            hopsSet.insert(endpointRC);
            hopsSet.insert(hops.begin(), hops.end());

            if (r->router_profiling().IsBadForPath(rc.pubkey, 1))
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

          if (const auto maybe = router->node_db()->GetRandom(filter))
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
      return router->now();
    }

    void
    Builder::Build(std::vector<RouterContact> hops, PathRole roles)
    {
      if (IsStopped())
        return;
      lastBuild = Now();
      const RouterID edge{hops[0].pubkey};
      if (not router->pathbuild_limiter().Attempt(edge))
      {
        LogWarn(Name(), " building too fast to edge router ", edge);
        return;
      }

      std::string path_shortName = "[path " + router->ShortName() + "-";
      path_shortName = path_shortName + std::to_string(router->NextPathBuildNumber()) + "]";
      auto path = std::make_shared<path::Path>(hops, GetWeak(), roles, std::move(path_shortName));
      LogInfo(Name(), " build ", path->ShortName(), ": ", path->HopsString());

      oxenc::bt_list_producer frames;
      auto& path_hops = path->hops;
      size_t last_len{0};
      for (size_t i = 0; i < path_hops.size(); i++)
      {
        bool lastHop = (i == (path_hops.size() - 1));
        const auto& nextHop = lastHop ? path_hops[i].rc.pubkey : path_hops[i + 1].rc.pubkey;
        SetupHopKeys(path_hops[i], nextHop);
        auto frame_str = CreateHopInfoFrame(path_hops[i]);
        if (last_len != 0)
          assert(
              frame_str.size()
              == last_len);  // all frames should be the same length...not sure what that is yet
        last_len = frame_str.size();
        frames.append(frame_str);
      }

      std::string dummy{last_len, '\0'};
      // append dummy frames; path build request must always have MAX_LEN frames
      for (size_t i = 0; i < path::MAX_LEN - path_hops.size(); i++)
      {
        randombytes(reinterpret_cast<uint8_t*>(dummy.data()), dummy.size());
        frames.append(dummy);
      }

      m_router->notify_router_event<tooling::PathAttemptEvent>(m_router->pubkey(), path);

      auto self = GetSelf();
      m_router->path_context().AddOwnPath(self, path);
      PathBuildStarted(path);

      auto response_cb = [self](oxen::quic::message m) {
        // TODO: this (replaces handling LRSM, which also needs replacing)
      };

      if (not m_router->send_control_message(
              path->Upstream(), "path_build", std::move(frames.str()), std::move(response_cb)))
      {
        log::warning(log_path, "Error sending path_build control message");
        path->EnterState(path::ePathFailed, m_router->now());
      }

      m_router->PersistSessionUntil(path->Upstream(), path->ExpireTime());
    }

    void
    Builder::HandlePathBuilt(Path_ptr p)
    {
      buildIntervalLimit = PATH_BUILD_RATE;
      router->router_profiling().MarkPathSuccess(p.get());

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
      router->router_profiling().MarkPathTimeout(p.get());
      PathSet::HandlePathBuildTimeout(p);
      DoPathBuildBackoff();
      for (const auto& hop : p->hops)
      {
        const RouterID router{hop.rc.pubkey};
        // look up router and see if it's still on the network
        router->loop()->call_soon([router, r = router]() {
          LogInfo("looking up ", router, " because of path build timeout");
          r->rc_lookup_handler().get_rc(
              router,
              [r](const auto& router, const auto* rc, auto result) {
                if (result == RCRequestResult::Success && rc != nullptr)
                {
                  LogInfo("refreshed rc for ", router);
                  r->node_db()->PutIfNewer(*rc);
                }
                else
                {
                  // remove all connections to this router as it's probably not registered anymore
                  LogWarn("removing router ", router, " because of path build timeout");
                  r->link_manager().deregister_peer(router);
                  r->node_db()->Remove(router);
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

  }  // namespace path
}  // namespace llarp
