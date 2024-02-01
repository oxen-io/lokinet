#include "pathset.hpp"

#include "path.hpp"

#include <llarp/crypto/crypto.hpp>

namespace llarp::path
{
    PathSet::PathSet(size_t num) : num_paths_desired(num)
    {}

    bool PathSet::ShouldBuildMore(llarp_time_t now) const
    {
        (void)now;
        const auto building = NumInStatus(PathStatus::BUILDING);
        if (building >= num_paths_desired)
            return false;
        const auto established = NumInStatus(PathStatus::ESTABLISHED);
        return established < num_paths_desired;
    }

    // bool
    // PathSet::ShouldBuildMoreForRoles(llarp_time_t now, PathRole roles) const
    // {
    //   Lock_t l(paths_mutex);
    //   const size_t required = MinRequiredForRoles(roles);
    //   size_t has = 0;
    //   for (const auto& item : _paths)
    //   {
    //     if (item.second->SupportsAnyRoles(roles))
    //     {
    //       if (!item.second->ExpiresSoon(now))
    //         ++has;
    //     }
    //   }
    //   return has < required;
    // }

    // size_t
    // PathSet::MinRequiredForRoles(PathRole roles) const
    // {
    //   (void)roles;
    //   return 0;
    // }

    size_t PathSet::NumPathsExistingAt(llarp_time_t futureTime) const
    {
        size_t num = 0;
        Lock_t l(paths_mutex);
        for (const auto& item : _paths)
        {
            if (item.second->IsReady() && !item.second->Expired(futureTime))
                ++num;
        }
        return num;
    }

    void PathSet::TickPaths(Router* r)
    {
        const auto now = llarp::time_now_ms();
        Lock_t l{paths_mutex};
        for (auto& item : _paths)
        {
            item.second->Tick(now, r);
        }
    }

    void PathSet::Tick(llarp_time_t)
    {
        std::unordered_set<RouterID> endpoints;
        for (auto& item : _paths)
        {
            endpoints.emplace(item.second->Endpoint());
        }

        path_cache.clear();
        for (const auto& ep : endpoints)
        {
            if (auto path = GetPathByRouter(ep))
            {
                path_cache[ep] = path->weak_from_this();
            }
        }
    }

    void PathSet::ExpirePaths(llarp_time_t now, [[maybe_unused]] Router* router)
    {
        Lock_t l(paths_mutex);
        if (_paths.size() == 0)
            return;
        auto itr = _paths.begin();
        while (itr != _paths.end())
        {
            if (itr->second->Expired(now))
            {
                // TODO: this
                PathID_t txid = itr->second->TXID();
                // router->outboundMessageHandler().RemovePath(std::move(txid));
                PathID_t rxid = itr->second->RXID();
                // router->outboundMessageHandler().RemovePath(std::move(rxid));
                itr = _paths.erase(itr);
            }
            else
                ++itr;
        }
    }

    std::shared_ptr<Path> PathSet::GetEstablishedPathClosestTo(
        RouterID id, std::unordered_set<RouterID> excluding, PathRole roles) const
    {
        Lock_t l{paths_mutex};
        std::shared_ptr<Path> path = nullptr;
        AlignedBuffer<32> dist;
        AlignedBuffer<32> to = id;
        dist.Fill(0xff);
        for (const auto& item : _paths)
        {
            if (!item.second->IsReady())
                continue;
            if (!item.second->SupportsAnyRoles(roles))
                continue;
            if (excluding.count(item.second->Endpoint()))
                continue;
            AlignedBuffer<32> localDist = item.second->Endpoint() ^ to;
            if (localDist < dist)
            {
                dist = localDist;
                path = item.second;
            }
        }
        return path;
    }

    std::shared_ptr<Path> PathSet::GetNewestPathByRouter(RouterID id, PathRole roles) const
    {
        Lock_t l(paths_mutex);
        std::shared_ptr<Path> chosen = nullptr;
        auto itr = _paths.begin();
        while (itr != _paths.end())
        {
            if (itr->second->IsReady() && itr->second->SupportsAnyRoles(roles))
            {
                if (itr->second->Endpoint() == id)
                {
                    // Note: revisit these, are they supposed to be redundant?
                    // if (chosen == nullptr)
                    //   chosen = itr->second;
                    // else if (chosen->intro.expiry < itr->second->intro.expiry)
                    //   chosen = itr->second;
                    chosen = itr->second;
                }
            }
            ++itr;
        }
        return chosen;
    }

    std::shared_ptr<Path> PathSet::GetPathByRouter(RouterID id, PathRole roles) const
    {
        Lock_t l(paths_mutex);
        std::shared_ptr<Path> chosen = nullptr;
        if (roles == ePathRoleAny)
        {
            if (auto itr = path_cache.find(id); itr != path_cache.end())
            {
                return itr->second.lock();
            }
        }
        auto itr = _paths.begin();
        while (itr != _paths.end())
        {
            if (itr->second->IsReady() && itr->second->SupportsAnyRoles(roles))
            {
                if (itr->second->Endpoint() == id)
                {
                    // Note: revisit these as well
                    // if (chosen == nullptr)
                    //   chosen = itr->second;
                    // else if (
                    //     chosen->intro.latency != 0s and chosen->intro.latency >
                    //     itr->second->intro.latency)
                    //   chosen = itr->second;
                    chosen = itr->second;
                }
            }
            ++itr;
        }
        return chosen;
    }

    std::shared_ptr<Path> PathSet::GetRandomPathByRouter(RouterID id, PathRole roles) const
    {
        Lock_t l(paths_mutex);
        std::vector<std::shared_ptr<Path>> chosen;
        auto itr = _paths.begin();
        while (itr != _paths.end())
        {
            if (itr->second->IsReady() && itr->second->SupportsAnyRoles(roles))
            {
                if (itr->second->Endpoint() == id)
                {
                    chosen.emplace_back(itr->second);
                }
            }
            ++itr;
        }
        if (chosen.empty())
            return nullptr;
        return chosen[std::uniform_int_distribution<size_t>{0, chosen.size() - 1}(llarp::csrng)];
    }

    std::shared_ptr<Path> PathSet::GetByEndpointWithID(RouterID ep, PathID_t id) const
    {
        Lock_t l(paths_mutex);
        auto itr = _paths.begin();
        while (itr != _paths.end())
        {
            if (itr->second->is_endpoint(ep, id))
            {
                return itr->second;
            }
            ++itr;
        }
        return nullptr;
    }

    std::shared_ptr<Path> PathSet::GetPathByID(PathID_t id) const
    {
        Lock_t l(paths_mutex);
        auto itr = _paths.begin();
        while (itr != _paths.end())
        {
            if (itr->second->RXID() == id)
                return itr->second;
            ++itr;
        }
        return nullptr;
    }

    size_t PathSet::AvailablePaths(PathRole roles) const
    {
        Lock_t l(paths_mutex);
        size_t count = 0;
        auto itr = _paths.begin();
        while (itr != _paths.end())
        {
            if (itr->second->Status() == PathStatus::ESTABLISHED
                && itr->second->SupportsAnyRoles(roles))
                ++count;
            ++itr;
        }
        return count;
    }

    size_t PathSet::NumInStatus(PathStatus st) const
    {
        Lock_t l(paths_mutex);
        size_t count = 0;
        auto itr = _paths.begin();
        while (itr != _paths.end())
        {
            if (itr->second->Status() == st)
                ++count;
            ++itr;
        }
        return count;
    }

    void PathSet::AddPath(std::shared_ptr<Path> path)
    {
        Lock_t l(paths_mutex);
        const auto upstream = path->upstream();  // RouterID
        const auto RXID = path->RXID();          // PathID
        if (not _paths.emplace(std::make_pair(upstream, RXID), path).second)
        {
            LogError(
                Name(),
                " failed to add own path, duplicate info wtf? upstream=",
                upstream,
                " rxid=",
                RXID);
        }
    }

    std::shared_ptr<Path> PathSet::GetByUpstream(RouterID remote, PathID_t rxid) const
    {
        Lock_t l(paths_mutex);
        auto itr = _paths.find({remote, rxid});
        if (itr == _paths.end())
            return nullptr;
        return itr->second;
    }

    std::optional<std::set<service::Introduction>> PathSet::GetCurrentIntroductionsWithFilter(
        std::function<bool(const service::Introduction&)> filter) const
    {
        std::set<service::Introduction> intros;
        Lock_t l{paths_mutex};
        auto itr = _paths.begin();
        while (itr != _paths.end())
        {
            if (itr->second->IsReady() and filter(itr->second->intro))
            {
                intros.insert(itr->second->intro);
            }
            ++itr;
        }
        if (intros.empty())
            return std::nullopt;
        return intros;
    }

    void PathSet::HandlePathBuildTimeout(std::shared_ptr<Path> p)
    {
        LogWarn(Name(), " path build ", p->short_name(), " timed out");
        build_stats.timeouts++;
    }

    void PathSet::HandlePathBuildFailedAt(std::shared_ptr<Path> p, RouterID hop)
    {
        LogWarn(Name(), " path build ", p->short_name(), " failed at ", hop);
        build_stats.fails++;
    }

    void PathSet::HandlePathDied(std::shared_ptr<Path> p)
    {
        LogWarn(Name(), " path ", p->short_name(), " died");
    }

    void PathSet::PathBuildStarted(std::shared_ptr<Path> p)
    {
        LogInfo(Name(), " path build ", p->short_name(), " started");
        build_stats.attempts++;
    }

    util::StatusObject BuildStats::ExtractStatus() const
    {
        return util::StatusObject{
            {"success", success}, {"attempts", attempts}, {"timeouts", timeouts}, {"fails", fails}};
    }

    std::string BuildStats::ToString() const
    {
        return fmt::format(
            "{:.2f} percent success (success={} attempts={} timeouts={} fails={})",
            SuccessRatio() * 100.0,
            success,
            attempts,
            timeouts,
            fails);
    }

    double BuildStats::SuccessRatio() const
    {
        if (attempts)
            return double(success) / double(attempts);
        return 0.0;
    }

    bool PathSet::GetNewestIntro(service::Introduction& intro) const
    {
        intro.Clear();
        bool found = false;
        Lock_t l(paths_mutex);
        auto itr = _paths.begin();
        while (itr != _paths.end())
        {
            if (itr->second->IsReady() && itr->second->intro.expiry > intro.expiry)
            {
                intro = itr->second->intro;
                found = true;
            }
            ++itr;
        }
        return found;
    }

    std::shared_ptr<Path> PathSet::PickRandomEstablishedPath(PathRole roles) const
    {
        std::vector<std::shared_ptr<Path>> established;
        Lock_t l(paths_mutex);
        auto itr = _paths.begin();
        while (itr != _paths.end())
        {
            if (itr->second->IsReady() && itr->second->SupportsAnyRoles(roles))
                established.push_back(itr->second);
            ++itr;
        }
        auto sz = established.size();
        if (sz)
        {
            return established[randint() % sz];
        }

        return nullptr;
    }

    std::shared_ptr<Path> PathSet::PickEstablishedPath(PathRole roles) const
    {
        std::vector<std::shared_ptr<Path>> established;
        Lock_t l(paths_mutex);
        auto itr = _paths.begin();
        while (itr != _paths.end())
        {
            if (itr->second->IsReady() && itr->second->SupportsAnyRoles(roles))
                established.push_back(itr->second);
            ++itr;
        }
        std::shared_ptr<Path> chosen = nullptr;
        llarp_time_t minLatency = 30s;
        for (const auto& path : established)
        {
            if (path->intro.latency < minLatency and path->intro.latency != 0s)
            {
                minLatency = path->intro.latency;
                chosen = path;
            }
        }
        return chosen;
    }

}  // namespace llarp::path
