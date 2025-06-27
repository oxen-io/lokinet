#include "path_handler.hpp"

#include "path.hpp"
#include "path_context.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/link/link_manager.hpp>
#include <llarp/messages/path.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/profiling.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/bspan.hpp>
#include <llarp/util/logging.hpp>

#include <sodium/randombytes.h>

#include <functional>
#include <random>

namespace llarp::path
{
    static auto logcat = log::Cat("pathhandler");

    bool BuildLimiter::Attempt(const RouterID& router) { return _edge_limiter.Insert(router); }

    void BuildLimiter::Decay(std::chrono::milliseconds now) { _edge_limiter.Decay(now); }

    bool BuildLimiter::Limited(const RouterID& router) const { return _edge_limiter.Contains(router); }

    nlohmann::json BuildStats::ExtractStatus() const
    {
        return nlohmann::json{
            {"success", success}, {"attempts", attempts}, {"timeouts", timeouts}, {"fails", build_fails}};
    }

    void BuildStats::update(std::chrono::milliseconds now)
    {
        if (attempts > 50 && attempts >= (success * 4) && now - last_warn_time > 5s)
        {
            log::warning(logcat, "Low path build success: {}", *this);
            last_warn_time = now;
        }
    }

    std::string BuildStats::to_string() const
    {
        return "Stats:[ success:{} | attempts:{} | timeouts:{} | fails:{} ]"_format(
            success, attempts, timeouts, build_fails);
    }

    PathHandler::PathHandler(Router& r, size_t num_paths, int n_hops)
        : _running{true}, num_paths_desired{num_paths}, _router{r}, num_hops{n_hops}
    {}

    static const std::shared_ptr<Path> NULL_PATH{nullptr};

    void PathHandler::path_rotation_succeeded(const std::shared_ptr<Path>& new_path)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        path_build_succeeded(std::move(new_path));
        drop_oldest_path();
    }

    static constexpr auto path_expiry_cmp = [](const auto& a, const auto& b) {
        return a.second->intro.expiry < b.second->intro.expiry;
    };
    const std::shared_ptr<Path>& PathHandler::get_oldest_path() const
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        Lock_t l{paths_mutex};
        if (_paths.empty())
            return NULL_PATH;
        return std::ranges::min_element(_paths, path_expiry_cmp)->second;
    }

    const std::shared_ptr<Path>& PathHandler::get_newest_path() const
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        Lock_t l{paths_mutex};
        if (_paths.empty())
            return NULL_PATH;
        return std::ranges::max_element(_paths, path_expiry_cmp)->second;
    }

    void PathHandler::add_path(std::shared_ptr<Path> p)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        Lock_t l(paths_mutex);

        _paths.insert_or_assign(p->edge().rxid(), p);
        _router.path_context.add_path(p);
    }

    void PathHandler::drop_path(const std::shared_ptr<Path>& p)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        Lock_t l{paths_mutex};

        if (auto itr = _paths.find(p->edge().rxid()); itr != _paths.end())
            _paths.erase(itr);

        _router.path_context.drop_path(*p);
    }

    const std::shared_ptr<Path>& PathHandler::get_random_path() const
    {
        if (_paths.empty())
            return NULL_PATH;
        int i = std::uniform_int_distribution<int>{0, static_cast<int>(_paths.size()) - 1}(csrng);
        return std::next(_paths.begin(), i)->second;
    }

    const std::shared_ptr<Path>& PathHandler::find_path(std::function<bool(const Path&)> filter) const
    {
        for (auto& p : _paths)
            if (filter(*p.second))
                return p.second;
        return NULL_PATH;
    }

    void PathHandler::tick_paths()
    {
        Lock_t l{paths_mutex};

        const auto now = llarp::time_now_ms();

        for (const auto& [h, p] : _paths)
            if (p)
                p->Tick(now);
    }

    void PathHandler::ping_paths(std::chrono::milliseconds now)
    {
        Lock_t l{paths_mutex};

        for (const auto& [h, p] : _paths)
            if (p)
                p->do_ping(now);
    }

    std::chrono::milliseconds PathHandler::now() const { return _router.now(); }

    void PathHandler::expire_paths(std::chrono::milliseconds now)
    {
        Lock_t lock{paths_mutex};

        std::vector<HopID> to_drop;

        for (auto itr = _paths.begin(); itr != _paths.end();)
        {
            if (itr->second and itr->second->is_established() and itr->second->is_expired(now))
            {
                to_drop.push_back(itr->second->edge().rxid());
                to_drop.push_back(itr->second->pivot().txid());
                itr = _paths.erase(itr);
            }
            else
                ++itr;
        }

        if (not to_drop.empty())
        {
            log::debug(logcat, "{} paths expired; giving path-ctx droplist", to_drop.size());
            _router.path_context.drop_paths(std::move(to_drop));
        }
    }

    const std::shared_ptr<Path>& PathHandler::get_path(HopID hid) const
    {
        Lock_t lock{paths_mutex};

        if (auto itr = _paths.find(hid); itr != _paths.end())
            return itr->second;

        return NULL_PATH;
    }

    void PathHandler::for_each_path(std::function<void(const Path&)> visit) const
    {
        Lock_t lock{paths_mutex};

        for (const auto& [_, p] : _paths)
            if (p)
                visit(*p);
    }

    sorted_intro_set PathHandler::get_local_client_intros() const
    {
        Lock_t lock{paths_mutex};

        sorted_intro_set intros{};
        auto now = llarp::time_now_ms();

        for (const auto& [_, p] : _paths)
        {
            if (p and p->is_active(now))
                intros.emplace(p->intro);
        }

        return intros;
    }

    void PathHandler::tick(std::chrono::milliseconds now)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        Lock_t l{paths_mutex};

        now = llarp::time_now_ms();
        _router.pathbuild_limiter().Decay(now);

        expire_paths(now);

        if (auto n = should_build_more(); n > 0)
            build_more(n);

        tick_paths();
        _build_stats.update(now);
    }

    nlohmann::json PathHandler::ExtractStatus() const
    {
        auto paths = nlohmann::json::array();
        for (auto& [h, path] : _paths)
            if (path)
                paths.push_back(path->ExtractStatus());

        return nlohmann::json{
            {"buildStats", _build_stats.ExtractStatus()},
            {"numHops", num_hops},
            {"numPaths", num_paths_desired},
            {"paths", std::move(paths)}};
    }

    std::optional<RemoteRC> PathHandler::select_first_hop(const std::unordered_set<RouterID>& exclude) const
    {
        std::unordered_set<RouterID> current_remotes = _router.node_db().strict_connect_enabled()
            ? _router.node_db().pinned_edges()
            : _router.get_current_remotes();

        RouterID edge;
        auto* out = std::ranges::sample(
            current_remotes | std::views::filter([this, &exclude](const RouterID& rid) {
                if (exclude.count(rid))
                    return false;
                if (build_cooldown_hit(rid))
                    return false;
                // always returns false on testnet builds
                if (_router.router_profiling().is_bad_for_path(rid))
                    return false;
                return true;
            }),
            &edge,
            1,
            csrng);

        if (out != &edge)
            return std::nullopt;
        if (auto* rc = _router.node_db().get_rc(edge))
            return *rc;
        return std::nullopt;
    }

    size_t PathHandler::num_active_paths() const
    {
        Lock_t l(paths_mutex);

        size_t n{};

        for (const auto& [_, p] : _paths)
        {
            if (p and p->is_active())
                n += 1;
        }

        return n;
    }

    size_t PathHandler::num_paths() const
    {
        Lock_t l(paths_mutex);

        return _paths.size();
    }

    void PathHandler::stop(bool)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        _running = false;

        if (_path_rotater)
        {
            if (_path_rotater->is_running())
                _path_rotater->stop();

            _path_rotater.reset();
            log::trace(logcat, "Path rotation ticker stopped!");
        }

        _paths.clear();
    }

    bool PathHandler::is_stopped() const { return !_running.load(); }

    bool PathHandler::should_remove() const { return is_stopped() and num_active_paths() == 0; }

    bool PathHandler::build_cooldown_hit(RouterID edge) const { return _router.pathbuild_limiter().Limited(edge); }

    bool PathHandler::build_cooldown() const { return llarp::time_now_ms() < last_build + build_interval_limit; }

    size_t PathHandler::should_build_more() const
    {
        if (is_stopped())
            return {};

        if (build_cooldown())
            return {};

        auto n_paths = num_paths();

        return num_paths_desired >= n_paths ? num_paths_desired - n_paths : 0;
    }

    std::optional<std::vector<RemoteRC>> PathHandler::get_hops_to_random()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (auto maybe = _router.node_db().get_random_rc([&r = _router](const RemoteRC& rc) {
                return not r.router_profiling().is_bad_for_path(rc.router_id(), 1);
            }))
            return aligned_hops_to_remote(maybe->router_id());

        return std::nullopt;
    }

    std::optional<std::vector<RemoteRC>> PathHandler::aligned_hops_to_remote(const RouterID& pivot)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        assert(num_hops);

        auto hops_needed = num_hops;

        auto hops = std::make_optional<std::vector<RemoteRC>>();

        auto* pivot_rc = _router.node_db().get_rc(pivot);
        if (!pivot_rc)
        {
            log::warning(logcat, "Failed to select aligned hops: no RC found for requested pivot {}", pivot);
            return std::nullopt;
        }

        if (--hops_needed <= 0)
        {
            // if we only need one hop then we're done!
            hops->push_back(std::move(*pivot_rc));
            return hops;
        }

        auto netmask = _router.config().paths.unique_hop_netmask;
        std::unordered_set<RouterID> to_exclude{{pivot}};
        std::vector<ipv4_net> excluded_ranges{};
        if (netmask)
            excluded_ranges.reserve(num_hops);

        auto exclude = [&netmask, &to_exclude, &excluded_ranges](const RemoteRC& rc) {
            to_exclude.insert(rc.router_id());
            if (netmask)
                excluded_ranges.push_back(rc.addr().to_ipv4() % netmask);
        };

        exclude(*pivot_rc);

        // First hop selection has its own distinct criteria:
        if (auto maybe = select_first_hop(to_exclude))
        {
            --hops_needed;
            hops->push_back(std::move(*maybe));
            exclude(hops->back());
        }
        else
        {
            log::warning(logcat, "No first hop candidate for aligned hops!");
            return std::nullopt;
        }

        log::trace(logcat, "First/last hop selected, {} hops remaining to select", hops_needed);

        auto filter =
            [&rp = _router.router_profiling(), &excluded_ranges, &to_exclude, &netmask](const RemoteRC& rc) -> bool {
            auto& rid = rc.router_id();
            if (to_exclude.contains(rid))
                return false;

            if (netmask)
            {
                auto v4 = rc.addr().to_ipv4();
                for (auto& r : excluded_ranges)
                    if (r.contains(v4))
                        return false;
            }

            if (rp.is_bad_for_path(rc.router_id(), 1))
                return false;

            return true;
        };
        for (; hops_needed > 0; hops_needed--)
        {
            // We can't use get_n_random_rcs here to select hops_needed all at once because as we
            // select each one that affects the selection criteria for the next one, not *only*
            // because of no-replacement but also because of the unique range setting.  (And we
            // can't use a mutating filter because the random selection potentially calls the filter
            // for every possible node, whether or not they end up being in the final selection).
            auto maybe_hop = _router.node_db().get_random_rc(filter);
            if (!maybe_hop)
            {
                log::warning(
                    logcat,
                    "Failed to find enough acceptable RCs for aligned path to pivot {}: {} required but only found {}",
                    pivot,
                    num_hops,
                    num_hops - hops_needed);
                return std::nullopt;
            }

            hops->push_back(std::move(*maybe_hop));
            auto& hop = hops->back();
            to_exclude.insert(hop.router_id());
            if (netmask)
                excluded_ranges.push_back(hop.addr().to_ipv4() % netmask);
        }

        log::debug(logcat, "Found {} RCs for aligned path to pivot {}", hops_needed, pivot);
        hops->push_back(*pivot_rc);
        return hops;
    }

    bool PathHandler::build_path_to_random()
    {
        Lock_t l(paths_mutex);

        if (auto maybe_hops = get_hops_to_random())
        {
            build(*maybe_hops);
            return true;
        }

        log::warning(logcat, "Failed to get hops for path-build to random");
        return false;
    }

    bool PathHandler::build_path_aligned_to_remote(const RouterID& remote)
    {
        Lock_t l(paths_mutex);

        if (auto maybe_hops = aligned_hops_to_remote(remote))
        {
            build(*maybe_hops);
            return true;
        }

        log::warning(logcat, "Failed to get hops for path-build to {}", remote);
        return false;
    }

    bool PathHandler::pre_build(const std::vector<RemoteRC>& hops)
    {
        if (is_stopped())
        {
            log::debug(logcat, "Path builder is stopped, aborting path build...");
            return false;
        }

        last_build = llarp::time_now_ms();
        const auto& edge = hops[0].router_id();

        if (not _router.pathbuild_limiter().Attempt(edge))
        {
            log::warning(logcat, "Building too quickly to edge router {}", edge);
            return false;
        }

        return true;
    }

    std::shared_ptr<Path> PathHandler::build1(const std::vector<RemoteRC>& hops)
    {
        auto path = std::make_shared<path::Path>(_router, hops, get_weak());

        {
            Lock_t l{paths_mutex};

            if (auto [it, b] = _paths.try_emplace(path->edge().rxid(), nullptr); not b)
            {
                log::debug(logcat, "Pending build to {} already underway... aborting...", path->edge().rxid());
                return nullptr;
            }
        }

        log::debug(logcat, "Building -> {}", path->to_string());

        return path;
    }

    std::string PathHandler::build2(const std::shared_ptr<Path>& path)
    {
        std::vector<std::string> frames(path::MAX_LEN);
        auto& path_hops = path->hops;
        int n_hops = static_cast<int>(path->num_hops);
        size_t last_len{0};

        // each hop will be able to read the outer part of its frame and decrypt
        // the inner part with that information.  It will then do an onion step on the
        // remaining frames so the next hop can read the outer part of its frame,
        // and so on.  As this de-onion happens from hop 1 to n, we create and onion
        // the frames from hop n downto 1 (i.e. reverse order).  The first frame is
        // not onioned.
        //
        // Onion-ing the frames in this way will prevent relays controlled by
        // the same entity from knowing they are part of the same path
        // (unless they're adjacent in the path; nothing we can do about that obviously).

        // i from n_hops down to 0
        for (int i = n_hops - 1; i >= 0; --i)
        {
            frames[i] = PATH::BUILD::serialize_hop(path_hops[i]);

            if (last_len and frames[i].size() != last_len)
            {
                assert(frames[i].size() == last_len);
                log::critical(logcat, "All frames must be the same length!");
            }

            last_len = frames[i].size();

            for (auto j = i + 1; j < n_hops; ++j)
            {
                auto _onion_nonce = path_hops[i].kx.nonce ^ path_hops[i].kx.xor_nonce;

                crypto::onion(as_bspan(frames[j]), path_hops[i].kx.shared_secret, _onion_nonce, _onion_nonce);
            }
        }

        // append dummy frames; path build request must always have MAX_LEN frames
        for (size_t i = n_hops; i < path::MAX_LEN; ++i)
        {
            frames[i].resize(last_len);
            randombytes_buf(reinterpret_cast<uint8_t*>(frames[i].data()), frames[i].size());
        }

        _build_stats.attempts++;

        return ONION::serialize_frames(std::move(frames));
    }

    void PathHandler::build(std::vector<RemoteRC> hops)
    {
        Lock_t lock{paths_mutex};

        // error message logs in function scope
        if (not pre_build(hops))
            return;

        if (auto new_path = build1(hops))
        {
            assert(new_path);

            path_build_onepass(
                std::move(new_path),
                [this](const std::shared_ptr<Path>& new_path) { path_build_succeeded(new_path); },
                [this](const std::shared_ptr<Path>& new_path, int ec) { return path_build_failed(new_path, ec); });
        }
    }

    void PathHandler::path_build_recursive(
        sorted_intro_set intros,
        NetworkAddress remote,
        std::function<void(const std::shared_ptr<Path>&, ClientIntro)> cb,
        bool keep_path)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        // we can recurse through this function as we remove the first pivot of the set of introductions every
        // invocation
        if (intros.empty())
        {
            log::warning(
                logcat, "Failed to build a path to remote {}: failed to connect to any published pivot", remote);
            return;
        }

        auto remote_intro = intros.extract(intros.begin()).value();

        auto& pivot = remote_intro.pivot_rid;

        log::debug(logcat, "Initiating recursive path-build to remote ({}) via pivot {}", remote, pivot.short_string());

        auto maybe_hops = aligned_hops_to_remote(pivot);

        if (not maybe_hops)
        {
            log::error(logcat, "Failed to get hops for path-build to pivot {}", pivot.short_string());
            return path_build_recursive(std::move(intros), std::move(remote), std::move(cb), keep_path);
        }

        auto& hops = *maybe_hops;
        assert(pivot == hops.back().router_id());

        std::shared_ptr<path::Path> new_path;

        if (keep_path)
        {
            new_path = build1(hops);

            if (not new_path)
            {
                log::warning(logcat, "Aborting recursive path-build in favor of in-progress build...");
                return;
            }
        }
        else
        {
            new_path = std::make_shared<path::Path>(_router, std::move(hops), get_weak());
            log::debug(logcat, "Building path -> {} :{}", new_path->to_string(), new_path->hop_string());
        }

        assert(new_path);

        auto on_success = [cb, remote_intro](const auto& new_path) mutable {
            return cb(new_path, std::move(remote_intro));
        };
        auto on_failure = [this, intros = std::move(intros), remote = std::move(remote), cb = std::move(cb), keep_path](
                              const auto& new_path, int ec) mutable {
            if (keep_path)
                path_build_failed(new_path, ec);
            path_build_recursive(std::move(intros), std::move(remote), std::move(cb), keep_path);
        };
        return path_build_onepass(std::move(new_path), std::move(on_success), std::move(on_failure));
    }

    void PathHandler::path_build_recursive(
        int n_tries,
        RemoteRC rc,
        NetworkAddress remote,
        std::function<void(const std::shared_ptr<Path>&)> cb,
        bool keep_path)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (n_tries == 0)
        {
            log::warning(logcat, "Exhausted all attempts to build path to router {}", remote);
            return;
        }

        log::debug(logcat, "Initiating iterative path-build (remaining attempts:{}) to router {}", n_tries, remote);

        auto maybe_hops = aligned_hops_to_remote(rc.router_id());

        if (not maybe_hops)
        {
            log::error(logcat, "Failed to get hops for path-build to router {}", remote);
            return path_build_recursive(--n_tries, std::move(rc), std::move(remote), std::move(cb), keep_path);
        }

        auto& hops = *maybe_hops;
        assert(rc.router_id() == hops.back().router_id());

        std::shared_ptr<path::Path> new_path;

        if (keep_path)
        {
            new_path = build1(hops);

            if (not new_path)
            {
                log::warning(logcat, "Aborting recursive path-build in favor of in-progress build...");
                return;
            }
        }
        else
        {
            new_path = std::make_shared<path::Path>(_router, std::move(hops), get_weak());
            log::debug(logcat, "Building path -> {} :{}", new_path->to_string(), new_path->hop_string());
        }

        assert(new_path);

        return path_build_onepass(
            std::move(new_path),
            [cb](auto new_path) mutable { return cb(std::move(new_path)); },
            [this, n_tries, rc = std::move(rc), remote = std::move(remote), cb, keep_path](
                auto new_path, int ec) mutable {
                if (keep_path)
                    path_build_failed(new_path, ec);
                path_build_recursive(--n_tries, std::move(rc), std::move(remote), std::move(cb), keep_path);
            });
    }

    void PathHandler::path_build_onepass(
        std::shared_ptr<Path> new_path, path_build_success_hook success_cb, path_build_fail_hook fail_cb)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        auto payload = build2(new_path);
        auto upstream = new_path->edge().router_id();

        if (!_router.send_control_message(
                std::move(upstream),
                "path_build",
                std::move(payload),
                [new_path, success_cb = std::move(success_cb), fail_cb](quic::message m) mutable {
                    if (m)
                    {
                        log::info(logcat, "PATH ESTABLISHED: {}", new_path->to_string());
                        return success_cb(std::move(new_path));
                    }

                    try
                    {
                        if (m.timed_out)
                        {
                            log::warning(logcat, "Path-build request timed out!");
                        }
                        else
                        {
                            oxenc::bt_dict_consumer d{m.body()};
                            auto status = d.require<std::string_view>(messages::STATUS_KEY);
                            log::warning(logcat, "Onepass path-build returned failure status: {}", status);
                        }
                    }
                    catch (const std::exception& e)
                    {
                        log::warning(
                            logcat, "Exception caught parsing path_build response: {}; input: {}", e.what(), m.body());
                    }

                    return fail_cb(std::move(new_path), m.timed_out);
                }))
        {
            log::warning(logcat, "Error sending path_build control message");
            return fail_cb(std::move(new_path), false);
        }
    }

    void PathHandler::rotate_paths(std::vector<RemoteRC> hops)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (auto new_path = build1(hops))
        {
            assert(new_path);

            log::debug(logcat, "Attempting path-rotation to new path...");
            path_build_onepass(
                std::move(new_path),
                [this](auto new_path) mutable { path_rotation_succeeded(std::move(new_path)); },
                [this](auto new_path, int ec) mutable { path_build_failed(std::move(new_path), ec); });
        }
    }

    void PathHandler::path_build_failed(const std::shared_ptr<Path>& p, bool timeout)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        drop_path(p);

        if (timeout)
        {
            _router.router_profiling().path_timeout(p.get());
            _build_stats.timeouts += 1;
        }
        else
            _build_stats.build_fails += 1;

        path_build_backoff();
    }

    void PathHandler::path_build_succeeded(const std::shared_ptr<Path>& p)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        p->set_established();
        add_path(p);
        build_interval_limit = PATH_BUILD_RATE;
        _router.router_profiling().path_success(p.get());
        _build_stats.success += 1;
    }

    void PathHandler::path_build_backoff()
    {
        static constexpr std::chrono::milliseconds MaxBuildInterval = 30s;
        // linear backoff
        build_interval_limit = std::min(PATH_BUILD_RATE + build_interval_limit, MaxBuildInterval);
        log::warning(logcat, "Build interval is now {}", build_interval_limit);
    }

    void PathHandler::path_died(const std::shared_ptr<Path>& p)
    {
        log::warning(logcat, "Path {} died post-build", p->to_string());
        _build_stats.path_fails++;
    }
}  // namespace llarp::path
