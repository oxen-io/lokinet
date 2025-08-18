#include "path_handler.hpp"

#include "llarp/constants/path.hpp"
#include "llarp/util/time.hpp"
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

#include <chrono>
#include <functional>

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

    PathHandler::PathHandler(Router& r, int target_paths, int num_hops)
        : router{r}, _running{true}, _num_hops{num_hops}, _target_paths{target_paths}
    {}

    static const std::shared_ptr<Path> NULL_PATH{nullptr};

    /*
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
    */

    void PathHandler::add_path(Path& p)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        Lock_t l(paths_mutex);

        _paths.insert_or_assign(p.edge().rxid, p.shared_from_this());
        router.path_context.add_path(p.shared_from_this());
    }

    void PathHandler::drop_path(const Path& p)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        _paths.erase(p.edge().rxid);

        router.path_context.drop_path(p);
    }

    /*
    Path* PathHandler::get_random_path() const
    {
        if (_paths.empty())
            return nullptr;
        return _paths
        int i = std::uniform_int_distribution<int>{0, static_cast<int>(_paths.size()) - 1}(csrng);
        return std::next(_paths.begin(), i)->second;
    }
    */

    void PathHandler::ping_paths(std::chrono::milliseconds now)
    {
        Lock_t l{paths_mutex};

        for (const auto& [h, p] : _paths)
            if (p)
                p->do_ping(now);
    }

    std::chrono::milliseconds PathHandler::now() const { return router.now(); }

    void PathHandler::expire_paths(std::chrono::milliseconds now)
    {
        Lock_t lock{paths_mutex};

        std::vector<HopID> to_drop;

        for (auto itr = _paths.begin(); itr != _paths.end();)
        {
            if (itr->second and itr->second->is_established() and itr->second->is_expired(now))
            {
                to_drop.push_back(itr->second->edge().rxid);
                to_drop.push_back(itr->second->terminus().txid);
                itr = _paths.erase(itr);
            }
            else
                ++itr;
        }

        if (not to_drop.empty())
        {
            log::debug(logcat, "{} paths expired; giving path-ctx droplist", to_drop.size());
            router.path_context.drop_paths(std::move(to_drop));
        }
    }

    Path* PathHandler::get_path_by_edge(const HopID& edge_hop_id)
    {
        if (auto it = _paths.find(edge_hop_id); it != _paths.end())
            return it->second.get();
        return nullptr;
    }

    Path* PathHandler::get_path_by_terminus(const HopID& terminal_hop_id)
    {
        for (auto& p : std::views::values(_paths))
            if (p && p->terminal_hopid() == terminal_hop_id)
                return p.get();
        return nullptr;
    }

    void PathHandler::tick(std::chrono::milliseconds now)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        Lock_t l{paths_mutex};

        router.pathbuild_limiter().Decay(now);

        expire_paths(now);

        if (!is_stopped())
            update_paths();

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
            {"numHops", _num_hops},
            {"targetPaths", _target_paths},
            {"paths", std::move(paths)}};
    }

    std::optional<RemoteRC> PathHandler::select_first_hop(std::function<bool(const RouterID&)> pred) const
    {
        std::unordered_set<RouterID> current_remotes =
            router.node_db().strict_connect_enabled() ? router.node_db().pinned_edges() : router.get_current_remotes();

        RouterID edge;
        auto* out = std::ranges::sample(
            current_remotes | std::views::filter([this, &pred](const RouterID& rid) {
                if (pred && !pred(rid))
                    return false;
                if (router.pathbuild_limiter().Limited(rid))
                    return false;
                // always returns false on testnet builds
                if (router.router_profiling().is_bad_for_path(rid))
                    return false;
                return true;
            }),
            &edge,
            1,
            csrng);

        if (out != (&edge + 1))
            return std::nullopt;
        if (auto* rc = router.node_db().get_rc(edge))
            return *rc;
        return std::nullopt;
    }

    int PathHandler::num_active_paths() const
    {
        Lock_t l(paths_mutex);

        int n = 0;
        for (const auto& [_, p] : _paths)
            if (p and p->is_active())
                n++;

        return n;
    }

    int PathHandler::num_paths(std::chrono::milliseconds expiry_ts) const
    {
        Lock_t l(paths_mutex);

        int n = 0;
        for (const auto& [_, p] : _paths)
            // TODO FIXME: what does a nullptr path mean?
            if (p and not p->is_expired(expiry_ts))
                n++;
        return n;
    }

    void PathHandler::stop()
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

    std::optional<std::vector<RemoteRC>> PathHandler::aligned_hops_to_remote(const RouterID& pivot)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        assert(_num_hops);

        int hops_needed = _num_hops;

        auto hops = std::make_optional<std::vector<RemoteRC>>();

        auto* pivot_rc = router.node_db().get_rc(pivot);
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

        auto netmask = router.config().paths.unique_hop_netmask;
        std::unordered_set<RouterID> to_exclude{{pivot}};
        std::vector<ipv4_net> excluded_ranges{};
        if (netmask)
            excluded_ranges.reserve(_num_hops);

        auto exclude = [&netmask, &to_exclude, &excluded_ranges](const RemoteRC& rc) {
            to_exclude.insert(rc.router_id());
            if (netmask)
                excluded_ranges.push_back(rc.addr().to_ipv4() % netmask);
        };

        exclude(*pivot_rc);

        // First hop selection has its own distinct criteria:
        auto maybe_first = select_first_hop([&to_exclude](const RouterID& rid) { return !to_exclude.contains(rid); });
        // If that failed, retry first hop selection *without* the IP range exclusion being applied:
        // this is so that if the pivot happens to be in the same range as all your current (or
        // allowed) edges, you can still connect to it.
        if (!maybe_first)
            maybe_first = select_first_hop();

        if (maybe_first)
        {
            --hops_needed;
            hops->push_back(std::move(*maybe_first));
            exclude(hops->back());
        }
        else
        {
            log::warning(logcat, "No first hop candidate for aligned hops!");
            return std::nullopt;
        }

        log::trace(logcat, "First/last hop selected, {} hops remaining to select", hops_needed);

        auto filter = [&rp = router.router_profiling(), &excluded_ranges, &to_exclude, &netmask](const RemoteRC& rc) {
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
            auto maybe_hop = router.node_db().get_random_rc(filter);
            if (!maybe_hop)
            {
                log::warning(
                    logcat,
                    "Failed to find enough acceptable RCs for aligned path to pivot {}: {} required but only found {}",
                    pivot,
                    _num_hops,
                    _num_hops - hops_needed);
                return std::nullopt;
            }

            hops->push_back(std::move(*maybe_hop));
            auto& hop = hops->back();
            to_exclude.insert(hop.router_id());
            if (netmask)
                excluded_ranges.push_back(hop.addr().to_ipv4() % netmask);
        }

        hops->push_back(*pivot_rc);
        return hops;
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

    bool PathHandler::can_build(std::span<const RemoteRC> hops)
    {
        if (is_stopped())
        {
            log::debug(logcat, "Path builder is stopped, aborting path build...");
            return false;
        }

        if (hops.empty())
        {
            log::error(logcat, "Error: cannot build an empty path!");
            return false;
        }

        if (hops.size() > path::BUILD_LENGTH)
        {
            log::error(
                logcat,
                "Error: cannot build a path of size {} (exceeds max path length {})",
                hops.size(),
                path::BUILD_LENGTH);
            return false;
        }

        _last_build = llarp::time_now_ms();
        const auto& edge = hops[0].router_id();

        if (not router.pathbuild_limiter().Attempt(edge))
        {
            log::warning(logcat, "Building too quickly to edge router {}", edge);
            return false;
        }

        return true;
    }

    std::shared_ptr<Path> PathHandler::build_init_path(std::span<const RemoteRC> hops)
    {
        auto path = std::make_shared<path::Path>(router, hops, *this);

        Lock_t l{paths_mutex};

        if (auto [it, b] = _paths.try_emplace(path->edge().rxid, path); not b)
        {
            // TODO FIXME: doesn't this mean we somehow selected an invalid rxid for the path
            // build, not that there is a path to the same remote?
            log::debug(logcat, "Pending build to {} already underway... aborting...", path->edge().rxid);
            return nullptr;
        }

        log::debug(logcat, "Building -> {}", path->to_string());

        return path;
    }

    static consteval size_t bt_pair_bytes(size_t value_len, size_t key_len = 1)
    {
        if (value_len > 999)
            throw std::invalid_argument{"value_len too big"};
        if (key_len > 9)
            throw std::invalid_argument{"key_len too big"};
        return (2 + key_len /*n:KEY*/) + (value_len < 10 ? 2 : value_len < 100 ? 3 : 4) + value_len /*NN:DATA*/;
    }
    static_assert(
        BUILD_FRAME_SIZE
        == 2 /*de*/ + bt_pair_bytes(PubKey::SIZE) /*k*/ + bt_pair_bytes(SymmNonce::SIZE) /*n*/ + /*x*/
            bt_pair_bytes(
                2 /*de*/ + bt_pair_bytes(HopID::SIZE) /*r*/ + bt_pair_bytes(HopID::SIZE) /*t*/
                + bt_pair_bytes(RouterID::SIZE) /*u*/));

    std::vector<std::byte> PathHandler::path_build_onion(Path& path)
    {
        // Note: we aren't using bt serialization here of the actual build frames, mainly because
        // one step of the build is to onion en/decrypt all following frame data, and that is much
        // simpler if it's just packed together without another serialization layer in it.

        if (not path.set_built())
            throw std::logic_error{"Cannot build a path from a Path object ({}) multiple times!"_format(path)};

        std::vector<std::byte> result;
        result.resize(BUILD_FRAME_SIZE * BUILD_LENGTH);
        std::span rspan{result};

        auto& path_hops = path.hops;
        int n_hops = static_cast<int>(path.num_hops());

        if (n_hops < BUILD_LENGTH)
        {
            // append junk data when our path is shorter than the max path length: path build
            // request must always have exactly BUILD_LENGTH frames
            random_fill(rspan.last(BUILD_FRAME_SIZE * (BUILD_LENGTH - n_hops)));
        }

        // each hop will be able to read the outer part of its frame and decrypt
        // the inner part with that information.  It will then do an onion step on the
        // remaining frame data so the next hop can read the outer part of its frame,
        // and so on.  As this de-onion happens from hop 1 to n, we create and onion
        // the frames from hop n downto 1 (i.e. reverse order).  The first frame is
        // not onioned.
        //
        // Onion-ing the frames in this way will prevent relays controlled by
        // the same entity from knowing they are part of the same path
        // (unless they're adjacent in the path; nothing we can do about that obviously).

        for (int i = n_hops - 1; i >= 0; --i)
        {
            /** For each hop:
                - Generate an Ed keypair for the hop (`shared_key`)
                - Generate a symmetric nonce for subsequent DH
                - Derive the shared secret (`hop.shared`) for DH key-exchange using the Ed keypair, hop pubkey, and
                    symmetric nonce
                - Encrypt the hop info in-place using `hop.shared` and the generated symmetric nonce from DH
                - Generate the XOR nonce by hashing the symmetric key from DH (`hop.shared`) and truncating

                Bt-encoded contents:
                - 'k' : ephemeral pubkey used to derive DH shared secret for this hop
                - 'n' : nonce used for DH secret calculation *and* for the encrypted payload (next item)
                - 'x' : encrypted payload
                    - 'r' : rxID (the path ID for messages going *to* the hop)
                    - 't' : txID (the path ID for messages coming *from* the client/path origin)
                    - 'u' : upstream hop RouterID

                All of these frames are inserted sequentially into the list and padded with any needed dummy frames
            */
            // TODO FIXME: poly1305 MAC for path build encryption
            auto& hop = path_hops[i];
            auto hop_payload = hop.bt_encode();

            auto dh_nonce = SymmNonce::make_random();
            auto eph_key = crypto::generate_ed25519();

            if (!crypto::dh_client(hop.shared_secret, hop.router_id, eph_key, dh_nonce))
                throw std::runtime_error{"Client DH failed for hop[{}] with rid {}"_format(i, hop.router_id)};

            hop.xor_nonce.assign(crypto::shorthash(hop.shared_secret).first<SymmNonce::SIZE>());

            crypto::xchacha20(as_bspan(hop_payload), hop.shared_secret, dh_nonce);

            oxenc::bt_dict_producer btdp;
            btdp.append("k", eph_key.pubkey_span());
            btdp.append("n", dh_nonce.span());
            btdp.append("x", hop_payload);
            auto frame = btdp.view();

            if (frame.size() != BUILD_FRAME_SIZE)
            {
                assert(frame.size() == BUILD_FRAME_SIZE);
                log::critical(logcat, "Internal error: unexpected path build frame size!");
                throw std::runtime_error{"Internal error: frame size mismatch in path build!"};
            }

            auto mine = rspan.subspan(i * BUILD_FRAME_SIZE, BUILD_FRAME_SIZE);
            std::memcpy(mine.data(), frame.data(), BUILD_FRAME_SIZE);

            if (auto following_frames = n_hops - 1 - i; following_frames > 0)
                // We only onion the real frames that follow this one, not the junk frames, because
                // the junk frames are never actually used.  When *de*-onioning we deonion all
                // following frames because we have no idea where the real frames end and junk
                // frames begin (because of frame rotation), which also has a nice side effect of
                // scrambling the junk frames so that junk values don't link path builds.  (This
                // also means the junk recovered isn't the same junk we produced, but that's fine).
                crypto::xchacha20(
                    rspan.subspan((i + 1) * BUILD_FRAME_SIZE, following_frames * BUILD_FRAME_SIZE),
                    hop.shared_secret,
                    dh_nonce ^ hop.xor_nonce);
        }

        _build_stats.attempts++;

        return result;
    }

    // TODO FIXME: investigate return type?
    int64_t PathHandler::build(std::span<const RemoteRC> hops)
    {
        Lock_t lock{paths_mutex};

        // error message logs in function scope
        if (can_build(hops))
        {
            if (auto new_path = build_init_path(hops))
            {
                auto id = ++_path_counter;
                send_path_build(std::move(new_path), id);
                // send_path_build calls the appropriate success/failure method
                return id;
            }
        }

        path_build_failed(0, nullptr, false);
        return 0;
    }

    /*
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
            new_path = std::make_shared<path::Path>(router, std::move(hops), *this);
            log::debug(logcat, "Building path -> {} :{}", new_path->to_string(), new_path->hop_string());
        }

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
            new_path = std::make_shared<path::Path>(router, std::move(hops), *this);
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
    */

    void PathHandler::send_path_build(const std::shared_ptr<Path>& new_path, int64_t id)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        auto payload = path_build_onion(*new_path);
        const auto& upstream = new_path->edge().router_id;

        router.send_control_message(
            std::move(upstream), "path_build", std::move(payload), [this, new_path, id](quic::message m) {
                if (m)
                {
                    log::info(logcat, "PATH ESTABLISHED: {}", *new_path);
                    return path_build_succeeded(id, *new_path);
                }

                if (m.timed_out)
                    log::warning(logcat, "Path-build request timed out!");
                else
                    try
                    {
                        oxenc::bt_dict_consumer d{m.body()};
                        auto status = d.require<std::string_view>(messages::STATUS_KEY);
                        log::warning(logcat, "Onepass path-build returned failure status: {}", status);
                    }
                    catch (const std::exception& e)
                    {
                        log::warning(
                            logcat,
                            "Exception caught parsing path_build response: {}; response body: {}",
                            e.what(),
                            llarp::buffer_printer{m.body()});
                    }

                return path_build_failed(id, new_path.get(), m.timed_out);
            });
    }

    void PathHandler::path_build_failed(int64_t build_id, Path* p, bool timeout)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (p)
            drop_path(*p);

        if (timeout)
        {
            if (p)
                router.router_profiling().path_timeout(*p);
            _build_stats.timeouts += 1;
        }
        else
            _build_stats.build_fails += 1;

        _last_failure = llarp::time_now_ms();
        _consecutive_failures++;

        on_path_build_failure(build_id, p, timeout);
    }

    void PathHandler::path_build_succeeded(int64_t build_id, Path& p)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        p.set_established();
        add_path(p);
        router.router_profiling().path_success(p);
        _build_stats.success += 1;

        _consecutive_failures = 0;

        on_path_build_success(build_id, p);
    }

    static constexpr auto MaxBuildInterval = 15s;

    bool PathHandler::cooldown(std::chrono::milliseconds now) const
    {
        if (_consecutive_failures < BACKOFF_THRESHOLD)
            return false;

        return now < _last_failure + BACKOFF_INCREMENT * (1 + _consecutive_failures - BACKOFF_THRESHOLD);
    }

    // TODO FIXME: something should be calling this!
    void PathHandler::path_died(const Path& p)
    {
        log::warning(logcat, "Path {} died post-build", p);
        _build_stats.path_fails++;
    }
}  // namespace llarp::path
