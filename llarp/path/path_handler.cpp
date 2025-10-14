#include "path_handler.hpp"

#include "path.hpp"
#include "path_context.hpp"

#include <llarp/constants/path.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/link/link_manager.hpp>
#include <llarp/messages/path.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/profiling.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/bspan.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/util/random.hpp>
#include <llarp/util/time.hpp>

#include <nlohmann/json.hpp>
#include <sodium/randombytes.h>

#include <chrono>
#include <functional>
#include <random>

namespace llarp::path
{
    static auto logcat = log::Cat("pathhandler");

    PathHandler::PathHandler(Router& r, int target_paths, int num_hops)
        : router{r}, _running{true}, _num_hops{num_hops}, _target_paths{target_paths}
    {}

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

        router.path_context.drop(p);
    }

    Path* PathHandler::get_random_active_path() const
    {
        int n_paths = num_active_paths();
        if (!n_paths)
            return nullptr;

        return &*std::next(active_paths().begin(), std::uniform_int_distribution<int>{0, n_paths - 1}(llarp::csrng));
    }

    void PathHandler::ping_paths(std::chrono::milliseconds now)
    {
        Lock_t l{paths_mutex};

        for (const auto& [h, p] : _paths)
            if (p)
                p->do_ping(now);
    }

    void PathHandler::expire_paths(std::chrono::milliseconds now)
    {
        Lock_t lock{paths_mutex};

        int n = 0;
        for (auto itr = _paths.begin(); itr != _paths.end();)
        {
            if (itr->second and itr->second->is_expired(now))
            {
                router.path_context.drop(*itr->second);
                itr = _paths.erase(itr);
                n++;
            }
            else
                ++itr;
        }

        if (n)
            log::debug(logcat, "{} expired paths dropped", n);
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

        expire_paths(now);

        if (not router.is_service_node and not router.is_connected())
            // If we are not yet fully connected then we can't initiate path builds.  (In theory we
            // could whe not yet fully connected, but don't want to because that would bias edge
            // router selection towards faster ones).
            return;

        if (!is_stopped())
            update_paths(now);

        router.path_builds.update(now);

        ping_paths(now);
    }

    nlohmann::json PathHandler::ExtractStatus() const
    {
        auto paths = nlohmann::json::array();
        for (auto& [h, path] : _paths)
            if (path)
                paths.push_back(path->ExtractStatus());

        return nlohmann::json{{"numHops", _num_hops}, {"targetPaths", _target_paths}, {"paths", std::move(paths)}};
    }

    const RelayContact* PathHandler::select_first_hop(std::function<bool(const RelayContact&)> pred) const
    {
#ifdef LOKINET_DEBUG_PATH_SEED
        auto current_remotes_unsorted =
#else
        auto current_remotes =
#endif
            router.link_endpoint().get_current_relays();

#ifdef LOKINET_DEBUG_PATH_SEED
        std::vector<RouterID> current_remotes;
        current_remotes.reserve(current_remotes_unsorted.size());
        current_remotes.assign(current_remotes_unsorted.begin(), current_remotes_unsorted.end());
        std::optional<std::mt19937_64> rng;
        if (router.config().paths.debug_path_seed)
        {
            rng.emplace(*router.config().paths.debug_path_seed);
            std::sort(current_remotes.begin(), current_remotes.end());
        }
#endif

        const RelayContact* selected = nullptr;
        int acceptable = 0;
        for (auto& rid : current_remotes)
        {
            auto* rc = router.node_db().get_rc(rid);
            if (!rc)
            {
                log::debug(logcat, "Skipping {}: missing RC", rid);
                continue;
            }

            if (pred && !pred(*rc))
                log::trace(
                    logcat, "Not considering {} for first hop selection because it failed the given predicate", rid);
            else if (router.router_profiling().is_bad_for_path(rid))  // always returns false on testnet
                log::trace(logcat, "Not considering {} for first hop because of router profiling", rid);
            else
            {
                log::trace(logcat, "Router {} is an acceptable first hop", rid);

                // DIY reservoir sample because doing this with a filter and a view calls the filter
                // code multiple times, which we don't want.
                if (acceptable == 0
                    || (
#ifdef LOKINET_DEBUG_PATH_SEED
                        rng ? std::uniform_int_distribution<int>{0, acceptable}(*rng) :
#endif
                            std::uniform_int_distribution<int>{0, acceptable}(llarp::csrng) == 0))
                    selected = rc;
                acceptable++;
            }
        }

        if (!selected)
            log::debug(logcat, "Failed to select first hop: no acceptable candidates found");

        return selected;
    }

    int PathHandler::num_active_paths(std::chrono::milliseconds expiry_ts) const
    {
        Lock_t l(paths_mutex);

        int n = 0;
        for (const auto& [_, p] : _paths)
            if (p and p->is_active() and not p->is_expired(expiry_ts))
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
            _path_rotater.reset();
            log::trace(logcat, "Path rotation ticker stopped!");
        }

        _paths.clear();
    }

    bool PathHandler::is_stopped() const { return !_running.load(); }

    std::optional<std::vector<RelayContact>> PathHandler::select_hops_to_remote(const RouterID& pivot)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        assert(_num_hops);

        int hops_needed = _num_hops;

        auto hops = std::make_optional<std::vector<RelayContact>>();

        auto* pivot_rc = router.node_db().get_rc(pivot);
        if (!pivot_rc)
        {
            log::warning(logcat, "Failed to select path hops: no RC found for requested pivot {}", pivot);
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

        auto exclude = [&netmask, &to_exclude, &excluded_ranges](const RelayContact& rc) {
            to_exclude.insert(rc.router_id());
            if (netmask)
                excluded_ranges.push_back(rc.addr().to_ipv4() % netmask);
        };

        exclude(*pivot_rc);

        auto filter =
            [&rp = router.router_profiling(), &excluded_ranges, &to_exclude, &netmask](const RelayContact& rc) {
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

        // Edge selection has its own distinct criteria: we can only select from edge routers we
        // have established connections to.  We take up to three passes to find a suitable edge:
        // 1. Try finding one with exclusion of `pivot`'s net range (when netmask exclusions are
        //    enabled).
        // 2. Try finding one that only excludes the pivot itself.
        // 3. Use the pivot as the first hop, and force path length of at least 3 (because
        //    client-A-A is not a valid path, so we need to force client-A-B-A).
        //
        // Ideally we want to avoid cases 2 or 3 but sometimes that is simply impossible: for
        // example if you need to build a path to A specifically, and A happens to be your only
        // pinned edge.  If we do find ourselves in case 2 or 3, we extend the path length by 1 (if
        // possible) to compensate for the repeated network in the path.
        //
        // We avoid cases 2/3 for inbound/utility paths by detecting when we have all edges in the
        // same range and avoiding that range for pivot selection.  For outbound paths it is
        // sometimes unavoidable because outbound paths need a specific terminus, leaving us with no
        // choice.
        const RelayContact* maybe_first = nullptr;
        bool extend_path = false;
        if (netmask)
        {
            maybe_first = select_first_hop(filter);

            if (!maybe_first)
                // We failed to find any edge that doesn't overlap with pivot's range, so extend the
                // path length by one to compensate.
                extend_path = true;
        }

        // Without a netmask filter, or if we couldn't find anything with the filter applied, open
        // up the selection to simply any relay that isn't the pivot itself:
        if (!maybe_first)
            maybe_first = select_first_hop([&pivot](const RelayContact& rc) { return rc.router_id() != pivot; });

        // If even that failed then we have to use the pivot itself.  This seems weird, but can
        // happen if you are connected to only a single router (e.g. with a pinned edge) *and* want
        // to build a path to that router.
        if (!maybe_first)
        {
            extend_path = true;
            maybe_first = router.node_db().get_rc(pivot);
        }

        // If that failed, retry first hop selection *without* the IP range exclusion being applied:
        // this is so that if the pivot happens to be in the same range as all your current (or
        // allowed) edges, you can still connect to it.
        if (!maybe_first)
            maybe_first = select_first_hop();

        if (!maybe_first)
        {
            log::warning(logcat, "No suitable first hop candidate for path to {}", pivot);
            return std::nullopt;
        }

        --hops_needed;
        hops->push_back(std::move(*maybe_first));
        exclude(hops->back());

        // If we're in two-hop mode, and went through the last selection fallback above, then we
        // could have just selected a path (client-A-A) but that is not valid: so in that special
        // case we forcible extend the path length by 1 to construct a path client-A-B-A instead.
        if (extend_path and hops_needed < BUILD_LENGTH - 2)
        {
            log::debug(
                logcat,
                "Extending path length from {} to {} because of unavoidable edge/terminus network overlap"
                " (edge: {}, terminus: {})",
                hops_needed + 2,
                hops_needed + 3,
                maybe_first->addr(),
                pivot_rc->addr());
            ++hops_needed;
        }

        log::trace(logcat, "First/last hop selected, {} hops remaining to select", hops_needed);

        for (; hops_needed > 0; hops_needed--)
        {
            // We can't use get_n_random_rcs here to select hops_needed all at once because as we
            // select each one that affects the selection criteria for the next one, not *only*
            // because of no-replacement but also because of the unique range setting.  (And we
            // can't use a mutating filter because the random selection potentially calls the filter
            // for every possible node, whether or not they end up being in the final selection).
            auto* maybe_hop = router.node_db().get_random_rc(filter);
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

    Path* PathHandler::build_path_to_remote(const RouterID& remote, std::chrono::seconds lifetime)
    {
        Lock_t l(paths_mutex);

        if (auto maybe_hops = select_hops_to_remote(remote))
        {
            return build(*maybe_hops, llarp::time_now_ms() + lifetime);
        }

        log::warning(logcat, "Failed to get hops for path-build to {}", remote);
        return nullptr;
    }

    bool PathHandler::can_build(std::span<const RelayContact> hops)
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

        return true;
    }

    std::shared_ptr<Path> PathHandler::build_init_path(
        std::span<const RelayContact> hops, std::chrono::milliseconds expiry_ts)
    {
        auto path = std::make_shared<path::Path>(router, hops, *this, expiry_ts);

        Lock_t l{paths_mutex};

        if (auto [it, b] = _paths.try_emplace(path->edge().rxid, path); not b)
        {
            // TODO FIXME: doesn't this mean we somehow selected an invalid rxid for the path
            // build, not that there is a path to the same remote?
            log::debug(logcat, "Pending build to {} already underway... aborting...", path->edge().rxid);
            return nullptr;
        }

        log::debug(logcat, "Building -> {}", *path);

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
                2 /*de*/ + bt_pair_bytes(sizeof(uint32_t)) /*l*/ + bt_pair_bytes(HopID::SIZE) /*r*/
                + bt_pair_bytes(HopID::SIZE) /*t*/ + bt_pair_bytes(RouterID::SIZE) /*u*/));

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

        auto path_expiry = oxenc::host_to_little(
            static_cast<uint32_t>(std::chrono::round<std::chrono::seconds>(path.expires_in()).count()));
        std::span<const std::byte, 4> path_expiry_encoded{
            reinterpret_cast<const std::byte*>(&path_expiry), sizeof(path_expiry)};

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
                    - 'l' : path lifetime in seconds, as a 4-byte, little-endian encoded integer (because we require an
               exact size)
                    - 'r' : rxID (the path ID for messages going *to* the hop)
                    - 't' : txID (the path ID for messages coming *from* the client/path origin)
                    - 'u' : upstream hop RouterID

                All of these frames are inserted sequentially into the list and padded with any needed dummy frames
            */
            // TODO FIXME: poly1305 MAC for path build encryption
            auto& hop = path_hops[i];

            std::string hop_payload;
            {
                oxenc::bt_dict_producer info;
                info.append("l", path_expiry_encoded);
                info.append("r", hop.rxid.to_view());
                info.append("t", hop.txid.to_view());
                info.append("u", hop.upstream.to_view());
                hop_payload = std::move(info).str();
            }

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

        router.path_builds.attempts++;

        return result;
    }

    // Constructs a TransitHop from a serialized path build frame, i.e. undoing one layer of the
    // path build onioning, above.  Returns the constructed TransitHop and the dh_nonce for the path
    // build.
    std::pair<std::shared_ptr<path::TransitHop>, SymmNonce> PathHandler::decrypt_build_frame(
        std::span<const std::byte, path::BUILD_FRAME_SIZE> frame,
        const Router& r,
        const std::variant<RouterID, quic::ConnectionID>& src,
        std::chrono::milliseconds now)
    {
        std::pair<std::shared_ptr<path::TransitHop>, SymmNonce> ret;
        auto& [hop_ptr, dh_nonce] = ret;
        auto& hop = *(hop_ptr = std::make_shared<path::TransitHop>());
        hop.downstream = src;

        PubKey eph_pubkey;
        std::vector<std::byte> payload;
        try
        {
            oxenc::bt_dict_consumer btdc{frame};
            eph_pubkey.assign(btdc.require_span<std::byte, PubKey::SIZE>("k"));
            dh_nonce.assign(btdc.require_span<std::byte, SymmNonce::SIZE>("n"));
            // Need to copy this because we decrypt in place below:
            auto payld = btdc.require<std::span<const std::byte>>("x");
            payload.assign(payld.begin(), payld.end());
            btdc.finish();
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception caught deserializing hop dict: {}", e.what());
            throw path::TransitHopError::INVALID_DATA();
        }

        if (!crypto::dh_server(hop.shared_secret, eph_pubkey, r.secret_key(), dh_nonce))
        {
            log::warning(logcat, "Failed to derive shared secret!");
            throw path::TransitHopError::DH_PUBKEY();
        }

        crypto::xchacha20(payload, hop.shared_secret, dh_nonce);
        hop.xor_nonce.assign(crypto::shorthash(hop.shared_secret).first<SymmNonce::SIZE>());

        try
        {
            oxenc::bt_dict_consumer inner{std::move(payload)};

            std::chrono::seconds lifetime{
                oxenc::load_little_to_host<uint32_t>(inner.require_span<std::byte, sizeof(uint32_t)>("l").data())};
            if (lifetime > path::MAX_LIFETIME_ACCEPTED)
                throw std::runtime_error{
                    "Path lifetime {} exceeds maximum allowed path lifetime {}"_format(lifetime, path::MAX_LIFETIME_ACCEPTED)};
            hop.expiry = now + lifetime;
            hop.rxid.assign(inner.require_span<std::byte, HopID::SIZE>("r"));
            hop.txid.assign(inner.require_span<std::byte, HopID::SIZE>("t"));
            hop.upstream.assign(inner.require_span<std::byte, RouterID::SIZE>("u"));
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "TransitHop caught bt parsing exception: {}", e.what());
            throw path::TransitHopError::INVALID_PAYLOAD();
        }

        // If we are a terminal hop then two things must be true: upstream must be this router, and
        // the rxid and txid must be equal.  If *not* a terminal hop, then both must be false.
        hop.terminal_hop = hop.upstream == r.id();
        bool terminal_mismatch = hop.terminal_hop != (hop.txid == hop.rxid);
        if (hop.txid.is_zero() || hop.rxid.is_zero() || terminal_mismatch)
            throw path::TransitHopError::INVALID_HOP_ID();

        log::trace(logcat, "TransitHop data successfully decrypted/deserialized: {}", hop);

        return ret;
    }

    // TODO FIXME: investigate return type?
    Path* PathHandler::build(std::span<const RelayContact> hops, std::chrono::milliseconds expiry_ts)
    {
        Lock_t lock{paths_mutex};

        // error message logs in function scope
        if (can_build(hops))
        {
            if (auto new_path = build_init_path(hops, expiry_ts))
            {
                auto ptr = new_path.get();
                auto id = ++_path_counter;
                send_path_build(std::move(new_path), id);
                // send_path_build calls the appropriate success/failure method
                return ptr;
            }
        }

        path_build_failed(0, nullptr, false);
        return nullptr;
    }

    void PathHandler::send_path_build(const std::shared_ptr<Path>& new_path, int64_t id)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        auto payload = path_build_onion(*new_path);
        const auto& upstream = new_path->edge().router_id;

        router.link_endpoint().send_command(
            upstream, "path_build", std::move(payload), [this, new_path, id](quic::message m) {
                if (m)
                {
                    log::info(logcat, "PATH ESTABLISHED: {}", *new_path);
                    log::trace(logcat, "path build response: {}", buffer_printer{m.body()});
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
            router.path_builds.timeouts++;
        }
        else
            router.path_builds.build_fails++;

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
        router.path_builds.success++;

        _consecutive_failures = 0;

        on_path_build_success(build_id, p);
    }

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
        router.path_builds.path_fails++;
    }
}  // namespace llarp::path
