#include "nodedb.hpp"

#include <llarp/crypto/types.hpp>
#include <llarp/link/link_manager.hpp>
#include <llarp/messages/fetch.hpp>
#include <llarp/util/file.hpp>
#include <llarp/util/random.hpp>
#include <llarp/util/time.hpp>
#include <llarp/util/zstd.hpp>

#include <oxen/quic/btstream.hpp>
#include <oxen/quic/loop.hpp>
#include <oxenc/base32z.h>
#include <sodium/crypto_generichash.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <iterator>
#include <random>
#include <ranges>
#include <unordered_map>
#include <utility>

namespace llarp
{
    static auto logcat = llarp::log::Cat("nodedb");

    std::array<int, 3> NodeDB::db_stats() const { return {num_rcs(), num_rids(), num_bootstraps()}; }

    template <typename RCContainer, std::predicate<const RelayContact&> Pred, typename RNG>
    static std::vector<const RelayContact*> sample_rcs(
        Router& router, const RCContainer& rcs, int n, const Pred& predicate, RNG& rng, bool shuffle)
    {
        auto now = llarp::time_now_ms();
        const auto& blacklist = router.config().paths.snode_blacklist;

        std::vector<const RelayContact*> rand;
        rand.resize(n);
        int admitted = 0;

        for (const RelayContact& rc : rcs)
        {
            if (rc.is_expired(now))
                continue;
            if (router.is_service_node)
            {
                if (rc.router_id() == router.id())
                    continue;
            }
            else if (blacklist.contains(rc.router_id()))
                continue;
            if (predicate and not predicate(rc))
                continue;

            auto pos = admitted < n ? admitted : std::uniform_int_distribution<int>{0, admitted}(rng);
            admitted++;
            if (pos < n)
                rand[pos] = &rc;
        }
        if (admitted < n)
            rand.resize(admitted);
        if (shuffle && rand.size() > 1)
            std::ranges::shuffle(rand, rng);
        return rand;
    }

    std::vector<const RelayContact*> NodeDB::get_n_random_rcs(
        int n, bool shuffle, const std::function<bool(const RelayContact&)>& predicate) const
    {
        assert(_router.loop.inside());
#ifdef LOKINET_DEBUG_PATH_SEED
        if (auto& s = _router.config().paths.debug_path_seed)
        {
            std::vector<std::reference_wrapper<const RelayContact>> rcs;
            rcs.reserve(known_rcs.size());
            for (const auto& rc : known_rcs | std::views::values)
                rcs.push_back(std::cref(rc));
            // We need a sorted list of known rcs because of the potentially non-reproducible order
            // of elements in an unordered map:
            std::ranges::sort(
                rcs, [](const RelayContact& a, const RelayContact& b) { return a.router_id() < b.router_id(); });
            std::mt19937_64 rng{*s};
            return sample_rcs(_router, rcs, n, predicate, rng, shuffle);
        }
#endif

        return sample_rcs(_router, known_rcs | std::views::values, n, predicate, llarp::csrng, shuffle);
    }

    const RelayContact* NodeDB::get_random_rc(const std::function<bool(const RelayContact&)>& predicate) const
    {
        auto randos = get_n_random_rcs(1, false, predicate);
        return randos.empty() ? nullptr : randos.front();
    }

    std::vector<const RelayContact*> NodeDB::get_n_random_edge_rcs(
        int n, bool shuffle, const std::function<bool(const RelayContact&)>& predicate) const
    {
        assert(_router.loop.inside());
        auto& strict = _router.config().paths.strict_edges;
        if (_router.is_service_node || strict.empty())
            return get_n_random_rcs(n, shuffle, predicate);

        n = std::min(n, static_cast<int>(strict.size()));

        // With strict edges the set of edges is typically small, so we iterate through just those
        // edges rather than sampling from *everything* with an "is in strict" lookups added to the
        // predicate.
        std::vector<std::reference_wrapper<const RelayContact>> strict_rcs;
        for (const auto& rid : strict)
            if (auto* rc = get_rc(rid))
                strict_rcs.emplace_back(std::cref(*rc));

#ifdef LOKINET_DEBUG_PATH_SEED
        if (auto& s = _router.config().paths.debug_path_seed)
        {
            std::ranges::sort(
                strict_rcs, [](const RelayContact& a, const RelayContact& b) { return a.router_id() < b.router_id(); });
            std::mt19937_64 rng{*s};
            return sample_rcs(_router, strict_rcs, n, predicate, rng, shuffle);
        }
#endif

        return sample_rcs(_router, strict_rcs, n, predicate, llarp::csrng, shuffle);
    }

    void NodeDB::bootstrap()
    {
        assert(_router.loop.inside());
        assert(!_bootstraps.empty());
        _bootstrap_running = true;

        if (_rc_fetch_ticker)
            _rc_fetch_ticker->stop();
        if (_rid_fetch_ticker)
            _rid_fetch_ticker->stop();

        struct bs_data
        {
            NodeDB& nodedb;
            size_t rc_i = 0;
            std::string body;
            RouterID source;

            // Shared pointer to ourself to keep us alive.  This is released once we run out of rcs,
            // or get a successful fetch.
            std::shared_ptr<void> keep_alive;

            void try_next()
            {
                if (nodedb._router.is_stopping())
                {
                    log::debug(logcat, "Aborting bootstrap because of router stop");
                    keep_alive.reset();
                    return;
                }

                if (rc_i >= nodedb._bootstraps.size())
                {
                    log::debug(logcat, "Bootstrapping failed: bootstraps list exhausted without any success");
                    auto ka = std::move(keep_alive);
                    nodedb.on_bootstrap_done(false);
                    return;
                }

                auto& rc = nodedb._bootstraps[rc_i++];
                source = rc.router_id();
                log::debug(
                    logcat,
                    "Initiating bootstrap request to {} @ {}",
                    rc.router_id().to_network_address(true),
                    rc.addr());
                auto [conn, control] = nodedb._router.link_endpoint().bootstrap_connect(rc);
                control->command("bfetch_rcs", body, [this, conn](quic::message m) {
                    nodedb._router.loop.call_soon([this, m = std::move(m)] {
                        if (not m)
                            log::warning(logcat, "Bootstrap fetch failed: {}", m.timed_out ? "timeout" : m.body());

                        else if (nodedb.handle_bootstrap_result(source, m.body()))
                        {
                            auto ka = std::move(keep_alive);
                            nodedb.on_bootstrap_done(true);
                            return;
                        }

                        try_next();
                    });

                    conn->close_connection();
                });
            }
        };
        auto bs = std::make_shared<bs_data>(*this);
        bs->keep_alive = bs;

        bs->body = "de";

        bs->try_next();
    }

    void NodeDB::purge_rcs(std::chrono::milliseconds now)
    {
        assert(_router.loop.inside());
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (_router.is_stopping() || not _router.is_running())
        {
            log::debug(logcat, "NodeDB unable to continue purge ticking -- router is stopped!");
            return;
        }

        remove_rcs_if([this, now](const RelayContact& rc) -> bool {
            // if for some reason we stored an RC that isn't a valid router
            // purge this entry
            if (not rc.addr().is_public())
            {
                log::trace(logcat, "Removing {}: address {} is not public", rc.router_id(), rc.addr());
                return true;
            }

            // clear out a fully expired RC
            if (rc.is_expired(now))
            {
                log::trace(logcat, "Removing {}: RC is expired", rc.router_id());
                return true;
            }

            if (_router.is_service_node)
            {
                // if we don't have the registered relay list yet don't remove the entry
                if (not has_registered_relays())
                {
                    log::trace(
                        logcat,
                        "Skipping check on {}: have not received oxend registered relay list yet",
                        rc.router_id());
                    return false;
                }

                if (not is_registered(rc.router_id()))
                {
                    log::trace(logcat, "Removing {}: not a valid router", rc.router_id());
                    return true;
                }
            }
            else
            {
                // Clients do not have an authoritative relay list, so we have no checks equivalent
                // to the above ones.
                log::trace(logcat, "Not removing {}: we are a client and it looks fine", rc.router_id());
                return false;
            }

            return false;
        });

        if (num_rcs() < MIN_ACTIVE_RCS and not _bootstraps.empty() and not _bootstrap_running)
        {
            log::warning(logcat, "Purging expired relays resulted in too few RCs; falling back to bootstrap mode");
            _bootstrap_fails = 0;
            bootstrap();
        }
    }

    std::filesystem::path NodeDB::get_path_by_pubkey(const RouterID& pubkey, const std::filesystem::path& ext) const
    {
        return _root / std::filesystem::path{pubkey.to_string()}.replace_extension(ext);
    }

    void NodeDB::fetch_rcs()
    {
        assert(_router.loop.inside());
        if (_router.is_stopping() || not _router.is_running())
        {
            log::debug(logcat, "NodeDB unable to continue RC fetch -- router is stopped!");
            if (_rc_fetch_ticker)
                _rc_fetch_ticker->stop();
            return;
        }

        std::vector<RouterID> to_fetch{};
        for (const auto& rid : known_rids)
        {
            if (!known_rcs.contains(rid))
                to_fetch.push_back(rid);

            if (to_fetch.size() == RC_FETCH_COUNT)
                break;
        }

        if (to_fetch.empty())
            return;

        path::Path* selected_path = _router.session_endpoint().get_random_active_path();
        if (!selected_path)
        {
            log::debug(logcat, "NodeDB fetch rcs, skipping because we have no paths.");
            return;
        }

        selected_path->fetch_relay_contacts(to_fetch, [this](quic::message m) mutable {
            std::string error;
            if (m)
            {
                try
                {
                    auto rcs = FetchRC::deserialize_response(_router.netid(), oxenc::bt_dict_consumer{m.body()});
                    log::debug(logcat, "RC fetching was successful; processing {} returned RCs...", rcs.size());
                    for (auto& rc : rcs)
                    {
                        const auto& rid = rc.router_id();
                        if (!put_rc(std::move(rc)))
                            log::debug(logcat, "Not inserting RC for {}, either it is newer or (if relay) ours", rid);
                    }
                }
                catch (const std::exception& e)
                {
                    error = e.what();
                }
            }
            else
            {
                error = m.timed_out ? "timed out" : "failed: {}"_format(m.body());
            }
        });
    }

    void NodeDB::fetch_rids()
    {
        assert(_router.loop.inside());
        if (_router.is_stopping() || not _router.is_running())
        {
            log::debug(logcat, "NodeDB skipping RouterID fetch -- router is stopped!");
            // FIXME: this *was* calling post_rid_fetch, but that seems wrong (and can segfault),
            //        might need to see *why* it was doing so, if for any logical reason
            return;
        }

        auto results = std::make_shared<std::unordered_map<RouterID, std::unordered_set<RouterID>>>();
        auto result_count = std::make_shared<size_t>(0);
        size_t try_count{0};
        std::vector<path::Path*> selected_paths;

        // In the future, we may want to make paths to selected sources for RID fetching,
        // but for now just use the first N paths that we already have for simplicity.  If
        // fetching fails from one, or not all results agree, we probably want to drop that
        // path anyway.
        for (auto& path : _router.session_endpoint().active_paths())
        {
            if (try_count >= RID_SOURCE_COUNT)
                break;
            auto [itr, inserted] = results->emplace(path.terminal_rid(), std::unordered_set<RouterID>{});
            if (inserted)
            {
                try_count++;
                selected_paths.push_back(&path);
            }
        }
        if (try_count < RID_SOURCE_COUNT)
            log::info(
                logcat,
                "Fetching RIDs from {} sources (want minimum {}, but not enough paths)",
                try_count,
                RID_SOURCE_COUNT);

        for (auto* path : selected_paths)
        {
            auto result_cb = [this, results, result_count, source = path->terminal_rid()](quic::message m) {
                (*result_count)++;
                if (not m)
                {
                    log::warning(
                        logcat,
                        "RID fetch from {} {}",
                        source,
                        m.timed_out ? "timed out" : "failed: {}"_format(m.body()));
                }
                else
                {
                    try
                    {
                        auto& router_ids = results->at(source);
                        oxenc::bt_dict_consumer btdc{m.body()};

                        btdc.required("r");

                        {
                            auto sublist = btdc.consume_list_consumer();

                            while (not sublist.is_finished())
                                router_ids.emplace(sublist.consume_span<uint8_t, 32>());
                        }
                    }
                    catch (const std::exception& e)
                    {
                        log::warning(logcat, "Error handling fetch RouterIDs response: {}", e.what());
                        results->at(source).clear();
                    }
                }
                if (*result_count == results->size())
                {
                    handle_fetched_router_ids(*results);
                }
            };
            path->send_path_control_message("fetch_rids"sv, {}, std::move(result_cb));
        }
    }

    void NodeDB::handle_fetched_router_ids(const std::unordered_map<RouterID, std::unordered_set<RouterID>>& results)
    {
        assert(_router.loop.inside());
        std::unordered_set<RouterID> accepted{};

        auto itr = results.begin();
        while (itr != results.end())
        {
            for (const auto& rid : itr->second)
            {
                size_t count{0};
                auto cur_itr = results.begin();
                while (cur_itr != results.end())
                {
                    if (cur_itr->second.contains(rid))
                        count++;
                    cur_itr++;
                }
                // FIXME: better than "half-rounded-up agree"
                if (count > (results.size() / 2))
                    accepted.insert(rid);
                else
                    log::info(logcat, "Received a RouterID that not enough nodes agree is correct: {}", rid);
            }
            itr++;
        }

        known_rids.clear();
        for (const auto& rid : accepted)
            known_rids.insert(rid);
    }

    void NodeDB::start()
    {
        log::trace(logcat, "NodeDB starting tickers...");

        _purge_ticker = _router.loop.call_every(PURGE_INTERVAL, [this] { purge_rcs(); });

        auto need_bootstrap = num_rcs() < MIN_ACTIVE_RCS;
        if (not has_bootstraps())
        {
            log::warning(logcat, "Only {} known RCs, but no bootstrap nodes are configured", num_rcs());
            need_bootstrap = false;
        }

        if (not _router.is_service_node)
        {
            _rc_fetch_ticker = _router.loop.call_every(
                FETCH_INTERVAL, [this] { fetch_rcs(); }, not need_bootstrap);

            _rid_fetch_ticker = _router.loop.call_every(
                FETCH_INTERVAL, [this] { fetch_rids(); }, not need_bootstrap);
        }

        _0rtt_saver = _router.disk_loop.make_wakeable([this] { _0rtt_save(); });
        _0rtt_saver->wake();

        if (need_bootstrap)
            bootstrap();
    }

    void NodeDB::on_bootstrap_done(bool success)
    {
        if (success)
        {
            log::debug(logcat, "Bootstrap attempt completed successfully");
            _bootstrap_fails = 0;
        }
        else
        {
            _bootstrap_fails++;
            log::debug(logcat, "Bootstrap attempt failed ({} consecutive failures)", _bootstrap_fails);
        }

        bool need_bootstrap = num_rcs() < MIN_ACTIVE_RCS;
        if (not need_bootstrap)
        {
            // TODO FIXME: we've now completed a bootstrap and so we want to fire off a full RID
            // fetch.  This current logic, however, doesn't seem right (but isn't specific to here):
            // we fire off an rid fetch *and* fire off an RC fetch back to back, on separate timers,
            // when really they should be dependent.
            //
            // But I'm not fixing it here because it needs a more significant overhaul.
            if (_rid_fetch_ticker)
            {
                _rid_fetch_ticker->start();
                fetch_rids();
            }
            if (_rc_fetch_ticker)
            {
                _rc_fetch_ticker->start();
                fetch_rcs();
            }
            return;
        }

        auto cooldown = std::min(BOOTSTRAP_COOLDOWN * (success ? 1 : _bootstrap_fails), BOOTSTRAP_COOLDOWN_MAX);
        log::warning(
            logcat,
            "Not enough RCs ({}) after {} bootstrap attempt; trying again in {}",
            num_rcs(),
            success ? "successful" : "failed",
            cooldown);
        _router.loop.call_later(cooldown, [this] { bootstrap(); });
    }

    NodeDB::NodeDB(Router& r) : _router{r}, _root{_router.config().router.data_dir / nodedb_dirname}
    {
        if (not exists(_root))
            create_directory(_root);
        if (not is_directory(_root))
            throw std::runtime_error{fmt::format("nodedb {} is not a directory", _root)};

        load_bootstraps();

        load_from_disk();

        if (_router.is_service_node)
        {
            // Make self.signed a symlink to the full ID.signed file.  (The full file might not
            // exist if we aren't a relay, but that's okay: we intentionally leave a dangling
            // symlink in that case).
            auto self_signed = _root / our_rc_filename;
            std::error_code ec;  // Ignore errors on the below as this is just for convenience but
                                 // doesn't otherwise matter.
            remove(self_signed, ec);
            create_symlink(
                std::filesystem::path{_router.id().to_string()}.replace_extension(RC_FILE_EXT), self_signed, ec);
        }
    }

    void NodeDB::load_bootstrap(const std::filesystem::path& fpath)
    {
        if (not exists(fpath))
            throw std::runtime_error{"Bootstrap RC file '{}' does not exist"_format(fpath)};

        auto content = util::file_to_string(fpath);
        if (content.empty())
            throw std::runtime_error{"Bootstrap RC file '{}' is empty"_format(fpath)};

        load_bootstrap(content, "Bootstrap RC file '{}'"_format(fpath));

        log::debug(logcat, "Successfully loaded BootstrapRC file {} ({}B)", fpath, content.size());
    }

    void NodeDB::load_bootstrap(std::string_view data, std::string_view input_desc)
    {
        try
        {
            // Bootstrap data can container either a list of bootstraps, or just a single bootstrap RC:
            if (data.front() == 'l')
            {
                // list of bootstrap RCs
                for (oxenc::bt_list_consumer l{data}; !l.is_finished();)
                    _bootstraps.emplace_back(l.consume_dict_data(), _router.netid(), /*accept_expired=*/true);
            }
            else
            {
                // single bootstrap RC
                _bootstraps.emplace_back(data, _router.netid(), /*accept_expired=*/true);
            }
        }
        catch (const std::exception& e)
        {
            log::debug(
                logcat, "Failed to load the following bootstrap data from {}: {}", input_desc, buffer_printer{data});
            throw std::runtime_error{"{} does not contain valid bootstrap data: {}"_format(input_desc, e.what())};
        }
    }

    void NodeDB::load_bootstraps()
    {
        const auto def = _router.config().router.data_dir / default_bootstrap;
        for (const auto& f : _router.config().bootstrap.files)
        {
            log::debug(logcat, "Loading BootstrapRC from file {}", f);
            load_bootstrap(f);
        }

        if (_bootstraps.empty() && exists(def))
        {
            log::debug(logcat, "No configured bootstraps; loading from {}", def);
            try
            {
                load_bootstrap(def);
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Failed loading from default bootstrap file {}: {}.  Skipping it.", def, e.what());
            }
        }

        auto obsolete = std::erase_if(_bootstraps, [](const auto& bs) { return bs.is_obsolete(); });
        if (obsolete > 0)
            log::info(logcat, "Removed {} obsolete bootstraps RCs", obsolete);

        if (std::erase_if(_bootstraps, [this](const auto& bs) { return bs.router_id() == _router.id(); }) > 0)
            log::info(logcat, "Found and removed ourself ({}) from the bootstrap list", _router.id());

        if (_bootstraps.empty())
        {
            log::debug(logcat, "Bootstrap list is empty; loading built-in fallbacks");
            for (const auto& [n, rc_blob] : bootstrap_fallbacks)
            {
                if (n == _router.netid())
                {
                    load_bootstrap(rc_blob, "Fallback bootstrap data");
                    break;
                }
            }

            log::info(
                logcat,
                "Loaded {} {} default fallback bootstrap router contact(s)",
                _bootstraps.size(),
                _router.netid());

            if (_bootstraps.empty())
            {
                log::warning(
                    logcat,
                    "No bootstrap router contacts were loaded.  The default bootstrap file {} does not "
                    "exist, and this lokinet binary does not have any fallback bootstraps for the '{}' network.",
                    def,
                    _router.netid());
            }
        }

        std::shuffle(_bootstraps.begin(), _bootstraps.end(), llarp::csrng);

        log::debug(logcat, "We have {} Bootstrap router(s)!", _bootstraps.size());
    }

    void NodeDB::post_rid_fetch(bool shutdown)
    {
        assert(_router.loop.inside());
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        fetch_counter = 0;
        response_counter = 0;
        fail_counter = 0;
        fail_sources.clear();
        rid_result_counters.clear();

        if (shutdown)
        {
            _rid_fetch_ticker->stop();
            log::warning(logcat, "Client stopped RouterID fetch without a sucessful response!");
        }
        else
            log::trace(logcat, "Client successfully completed RouterID fetch!");
    }

    bool NodeDB::handle_bootstrap_result(const RouterID& source, std::string_view body)
    {
        assert(_router.loop.inside());
        log::debug(logcat, "Received response to BootstrapRC fetch request...");

        int num = 0, n_new = 0;

        try
        {
            oxenc::bt_dict_consumer btdc{body};

            auto compressed_rcs = btdc.require_span<std::byte>("Z");

            btdc.finish();

            zstd::decompressor decompressor;
            // 20M here is just a safety margin so that a malicious bootstrap can't feed us some
            // tiny data that decompresses into something that exhausts memory:
            auto rcs_data = zstd::decompressor{}.decompress(compressed_rcs, 20'000'000);
            if (!rcs_data)
                throw std::runtime_error{"Failed to decompress RC list"};

            oxenc::bt_list_consumer rclist{*rcs_data};

            while (not rclist.is_finished())
            {
                RelayContact new_rc{rclist.consume_dict_data(), _router.netid()};
                // if we're trusting the bootstrap for RCs regardless of RouterID, we
                // should trust the RouterID as well.
                known_rids.insert(new_rc.router_id());
                n_new += put_rc(std::move(new_rc));
                ++num;
            }
        }
        catch (const std::exception& e)
        {
            log::warning(
                logcat, "Failed to parse BootstrapRC fetch response from {}: {}", source.short_string(), e.what());
            return false;
        }

        log::info(logcat, "Bootstrap fetch successfully retrieved {} RCs ({} new)", num, n_new);
        return true;
    }

    bool NodeDB::has_registered_relays() const
    {
        std::shared_lock lock{_registered_relays_mutex};
        return not _registered_relays.empty();
    }

    void NodeDB::set_registered_relays(std::unordered_set<RouterID> relays)
    {
        log::debug(logcat, "Oxend provided {} whitelisted routers", relays.size());

        if (relays.empty())
            return;

        size_t size = relays.size();
        {
            std::unique_lock lock{_registered_relays_mutex};
            std::swap(relays, _registered_relays);
        }

        if (relays.empty())
            log::info(logcat, "Loaded initial SN list from oxend with {} registered relays", size);
        else
            log::debug(logcat, "Updated SN list from oxend with {} registered relays", size);
    }

    void NodeDB::load_registered_relays_fallback()
    {
        std::unique_lock lock{_registered_relays_mutex};

        if (not _registered_relays.empty())
        {
            // Perhaps a race with a result fetch?
            log::debug(logcat, "Not loading registered relay fallback: we already have registered relays");
            return;
        }

        auto now = llarp::time_now_ms();
        for (auto& [rid, rc] : known_rcs)
            if (!rc.is_expired(now))
                _registered_relays.insert(rid);
    }

    std::vector<RouterID> NodeDB::get_registered_relays() const
    {
        std::vector<RouterID> result;
        std::shared_lock lock{_registered_relays_mutex};
        result.reserve(_registered_relays.size());
        result.assign(_registered_relays.begin(), _registered_relays.end());
        return result;
    }

    bool NodeDB::is_registered(const RouterID& relay) const
    {
        std::shared_lock lock{_registered_relays_mutex};
        return _registered_relays.contains(relay);
    }

    std::optional<RouterID> NodeDB::get_random_registered_relay() const
    {
        std::optional<RouterID> result;
        std::shared_lock lock{_registered_relays_mutex};
        if (!_registered_relays.empty())
            result = *std::next(
                _registered_relays.begin(),
                std::uniform_int_distribution<int>{0, static_cast<int>(_registered_relays.size())}(llarp::csrng));
        return result;
    }

    void NodeDB::load_from_disk()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (_root.empty())
            return;

        std::vector<std::filesystem::path> purge;

        const auto now = time_now_ms();
        const auto real_now = std::chrono::system_clock::now();

        for (const auto& f : std::filesystem::directory_iterator{_root})
        {
            if (not f.is_regular_file())
                continue;

            RouterID filename_rid;
            // Ignoring anything that doesn't look like PUBKEY.ext:
            if (auto no_ext = f.path().stem().u8string(); no_ext.size() == oxenc::to_base32z_size(RouterID::SIZE)
                and oxenc::is_base32z(no_ext.begin(), no_ext.end()))
            {
                oxenc::from_base32z(no_ext.begin(), no_ext.end(), filename_rid.begin());
            }
            else
            {
                log::debug(logcat, "Skipping {}: does not match PUBKEY.(ext) filename format", f.path());
                continue;
            }

            auto ext = f.path().extension();
            if (ext == RC_FILE_EXT)
            {
                std::optional<RelayContact> rc;
                try
                {
                    rc.emplace(f.path(), _router.netid());
                }
                catch (const std::exception& e)
                {
                    log::warning(logcat, "Failed to load {} from stored RCs: {}", f.path(), e.what());
                }

                if (not rc or rc->is_expired(now))
                {
                    // try loading it, purge it if it is junk or expired
                    purge.push_back(f);
                    continue;
                }

                auto rid = rc->router_id();

                if (rid != filename_rid)
                {
                    log::error(
                        logcat,
                        "Invalid stored RC: RC contains pubkey {} which does not match filename {}",
                        rid,
                        f.path());
                    purge.push_back(f);
                    continue;
                }

                if (not _router.is_service_node)
                    known_rids.insert(rid);  // Only clients use this container
                known_rcs.emplace(std::move(rid), std::move(*rc));
            }
            else if (ext == ZRTT_FILE_EXT)
            {
                std::string content;
                try
                {
                    content = util::file_to_string(f.path());
                    oxenc::bt_dict_consumer top{content};
                    RouterID rid{top.require_span<std::byte, RouterID::SIZE>("@")};
                    if (rid != filename_rid)
                    {
                        log::error(
                            logcat, "Invalid stored 0RTT ticket: pubkey {} does not match filename {}", rid, f.path());
                        purge.push_back(f);
                        continue;
                    }

                    std::list<std::pair<std::vector<unsigned char>, std::chrono::sys_seconds>> tickets;
                    auto recs = top.consume_list_consumer();
                    while (!recs.is_finished())
                    {
                        auto rec = recs.consume_dict_consumer();
                        auto data_sp = rec.require_span<unsigned char>("d");
                        std::vector<unsigned char> data{data_sp.begin(), data_sp.end()};
                        std::chrono::sys_seconds expiry{std::chrono::seconds{rec.require<int64_t>("e")}};
                        if (expiry > real_now)
                            tickets.emplace_back(std::move(data), expiry);
                        rec.finish();
                    }
                    top.finish();

                    if (tickets.empty())
                    {
                        // Everything expired
                        log::debug(logcat, "Deleting 0RTT ticket file {}: no unexpired tickets found", f.path());
                        purge.push_back(f);
                        continue;
                    }
                    _0rtt_tickets[rid] = std::move(tickets);
                }
                catch (const std::exception& e)
                {
                    log::warning(
                        logcat, "Failed to load {} from stored 0RTT data: {}; deleting it.", f.path(), e.what());
                    purge.push_back(f);
                    continue;
                }
            }
            else
                log::trace(logcat, "Skipping file with unknown extension: {}", f.path());
        }

        if (not purge.empty())
        {
            log::info(logcat, "removing {} invalid or expired RC/0RTT files from disk", purge.size());
            for (const auto& fpath : purge)
                remove(fpath);
        }

        log::info(
            logcat, "Loaded {} RCs + 0-RTT tickets for {} relays from disk", known_rcs.size(), _0rtt_tickets.size());
    }

    void NodeDB::cleanup()
    {
        if (_rid_fetch_ticker)
        {
            log::trace(logcat, "NodeDB clearing rid fetch ticker...");
            _rid_fetch_ticker->stop();
            _rid_fetch_ticker.reset();
        }

        if (_rc_fetch_ticker)
        {
            log::trace(logcat, "NodeDB clearing RC fetch ticker...");
            _rc_fetch_ticker->stop();
            _rc_fetch_ticker.reset();
        }

        if (_purge_ticker)
        {
            log::trace(logcat, "NodeDB clearing purge ticker...");
            _purge_ticker->stop();
            _purge_ticker.reset();
        }

        log::debug(logcat, "NodeDB cleared all tickers...");
    }

    const RelayContact* NodeDB::get_rc(const RouterID& pk) const
    {
        assert(_router.loop.inside());
        auto it = known_rcs.find(pk);
        return it != known_rcs.end() ? &it->second : nullptr;
    }

    bool NodeDB::put_rc(RelayContact rc)
    {
        assert(_router.loop.inside());

        auto [it, new_rc] = known_rcs.try_emplace(rc.router_id(), std::move(rc));
        auto& stored = it->second;
        bool should_gossip;
        if (new_rc)
        {
            // If this is a brand new RC then we want to gossip it to make sure everyone gets it.
            should_gossip = true;
        }
        else if (!rc.newer_than(stored, RelayContact::MIN_GOSSIP_RC_AGE))
        {
            // The RC is too new since the last one we stored, so drop it.
            return false;
        }
        else
        {
            // This RC is an update of one we already have: we only gossip if this RC indicates a
            // changed address (e.g. port or IP change) or was the first RC from this node in a long
            // time, both of which are updates we want to waste a little extra network bandwidth for
            // to get out everywhere ASAP via gossipping.  Otherwise it's a mundane update, and so
            // we don't gossip it because the full-mesh network connections means it will send it
            // directly to everyone (and other nodes don't need to update to be able to full mesh
            // with it).
            //
            // For our own RC, we always return true if we get here because we always want to gossip
            // our *own* RC whenever it gets updated.
            should_gossip = rc.router_id() == _router.id() || rc.newer_than(stored, RelayContact::OUTDATED_AGE)
                || rc.address_changed(stored);
            stored = std::move(rc);
        }

        // We inserted or updated the record, so queue saving it to disk on the disk loop
        _router.disk_loop.call_soon([rc = stored, path = get_path_by_pubkey(rc.router_id())] { rc.write(path); });

        return should_gossip;
    }

    bool NodeDB::verify_store_gossip_rc(RelayContact rc)
    {
        assert(_router.loop.inside());
        if (not is_registered(rc.router_id()) || rc.router_id() == _router.id())
            return false;
        return put_rc(std::move(rc));
    }

    int NodeDB::num_rcs(bool include_self) const
    {
        assert(_router.loop.inside());
        int total = static_cast<int>(known_rcs.size());
        if (not include_self and _router.is_service_node and known_rcs.contains(_router.id()))
            --total;
        return total;
    }

    int NodeDB::num_rids() const
    {
        assert(_router.loop.inside());
        if (_router.is_service_node)
        {
            std::shared_lock lock{_registered_relays_mutex};
            return static_cast<int>(_registered_relays.size());
        }
        return static_cast<int>(known_rids.size());
    }

    void NodeDB::remove_rcs_if(const std::function<bool(const RelayContact&)>& remove)
    {
        assert(_router.loop.inside());

        std::vector<RouterID> removed;

        for (auto it = known_rcs.begin(); it != known_rcs.end();)
        {
            const auto& [rid, rc] = *it;
            if (remove(rc))
            {
                removed.push_back(rid);
                it = known_rcs.erase(it);
            }
            else
                ++it;
        }

        if (not removed.empty())
            remove_many_from_disk_async(std::move(removed));
    }

    void NodeDB::remove_many_from_disk_async(const std::vector<RouterID>& remove) const
    {
        assert(_router.loop.inside());
        if (_root.empty())
            return;

        // build file list
        std::vector<std::filesystem::path> files;
        files.reserve(remove.size());
        for (const auto& rid : remove)
            files.push_back(get_path_by_pubkey(rid));

        // remove them from the disk via the diskio thread
        _router.disk_loop.call_soon([files = std::move(files)] {
            for (const auto& p : files)
                std::filesystem::remove(p);
        });
    }

    namespace
    {
        inline uint64_t xor_condense(const AlignedBuffer<32>& x)
        {
            auto* y = reinterpret_cast<const uint64_t*>(x.data());
            return y[0] ^ y[1] ^ y[2] ^ y[3];
        }

        // Metric for determining the "closest" router ID to a given blinded pubkey, used for
        // blinded CC publishing.
        //
        // This consists of xoring all of the uint64_t chunks of the blinded pubkey with the router
        // ID and returning the smallest value.
        struct PublishLocationMetric
        {
            PublishLocationMetric(const PubKey& blinded_pk) : pk_xor{xor_condense(blinded_pk)} {}

            const uint64_t pk_xor;  // xor of the blinded PK
            bool operator()(const RouterID* left, const RouterID* right) const
            {
                auto l = xor_condense(*left) ^ pk_xor;
                auto r = xor_condense(*right) ^ pk_xor;
                return std::tie(l, *left) < std::tie(r, *right);
            }
        };
    }  // namespace

    std::vector<RouterID> NodeDB::find_many_closest_to(const PubKey& blinded_pk, int num_routers) const
    {
        assert(_router.loop.inside());
        if (num_routers <= 0)
            return {};

        std::shared_lock lock{_registered_relays_mutex};
        auto rr = _registered_relays | std::views::transform([](const auto& rid) { return &rid; });
        std::vector<const RouterID*> rids{rr.begin(), rr.end()};
        num_routers = std::min(num_routers, static_cast<int>(rids.size()));
        std::ranges::partial_sort(rids, rids.begin() + num_routers, PublishLocationMetric{blinded_pk});
        rids.resize(num_routers);
        std::vector<RouterID> result;
        result.reserve(rids.size());
        for (auto* rid : rids)
            result.push_back(*rid);
        return result;
    }

    void NodeDB::store_0rtt(const RouterID& rid, std::vector<unsigned char> data, std::chrono::sys_seconds expiry)
    {
        std::lock_guard lock{_0rtt_mutex};
        auto& tickets = _0rtt_tickets[rid];
        while (tickets.size() >= MAX_0RTT_TICKETS)
            tickets.pop_front();
        tickets.emplace_back(std::move(data), expiry);
        _0rtt_dirty.insert(rid);
        _0rtt_saver->wake();
    }

    std::optional<std::vector<unsigned char>> NodeDB::extract_0rtt(const RouterID& rid)
    {
        std::lock_guard lock{_0rtt_mutex};
        std::optional<std::vector<unsigned char>> ret;
        auto now = std::chrono::system_clock::now();
        auto it = _0rtt_tickets.find(rid);
        if (it != _0rtt_tickets.end())
        {
            auto& tickets = it->second;
            // Delete any expired tickets:
            while (!tickets.empty() && tickets.front().second < now)
                tickets.pop_front();
            if (!tickets.empty())
            {
                ret = std::move(tickets.front().first);
                tickets.pop_front();
            }
            _0rtt_dirty.insert(rid);
            _0rtt_saver->wake();
        }
        return ret;
    }

    void NodeDB::_0rtt_save()
    {
        std::list<std::pair<std::filesystem::path, std::string>> rewrite;
        std::list<std::filesystem::path> erase;

        {
            std::lock_guard lock{_0rtt_mutex};
            auto now = std::chrono::system_clock::now();
            for (const auto& rid : _0rtt_dirty)
            {
                auto path = get_path_by_pubkey(rid, ZRTT_FILE_EXT);
                auto it = _0rtt_tickets.find(rid);
                if (it == _0rtt_tickets.end())
                    erase.push_back(std::move(path));
                else
                {
                    auto& tickets = it->second;
                    // Delete any expired tickets:
                    while (!tickets.empty() && tickets.front().second < now)
                        tickets.pop_front();

                    if (!tickets.empty())
                    {
                        oxenc::bt_dict_producer top;
                        top.append("@", rid.span());
                        auto recs = top.append_list("r");
                        for (const auto& [data, exp] : tickets)
                        {
                            auto e = recs.append_dict();
                            e.append("d", std::span{reinterpret_cast<const std::byte*>(data.data()), data.size()});
                            e.append("e", exp.time_since_epoch().count());
                        }
                        rewrite.emplace_back(std::move(path), std::move(top).str());
                        ++it;
                    }
                    else
                    {
                        erase.push_back(std::move(path));
                        it = _0rtt_tickets.erase(it);
                    }
                }
            }
            _0rtt_dirty.clear();
        }

        for (const auto& path : erase)
        {
            try
            {
                std::filesystem::remove(path);
            }
            catch (const std::exception& e)
            {
                log::error(logcat, "Failed to remove expired 0RTT ticket file {}: {}", path, e.what());
            }
        }
        for (const auto& [path, data] : rewrite)
        {
            try
            {
                util::buffer_to_file(path, data);
            }
            catch (const std::exception& e)
            {
                log::error(logcat, "Failed to update 0RTT ticket file {}: {}", path, e.what());
            }
        }
    }

}  // namespace llarp
