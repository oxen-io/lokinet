#include "nodedb.hpp"

#include <llarp/crypto/types.hpp>
#include <llarp/link/link_manager.hpp>
#include <llarp/messages/fetch.hpp>
#include <llarp/util/random.hpp>
#include <llarp/util/time.hpp>

#include <oxen/quic/btstream.hpp>
#include <sodium/crypto_generichash.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <iterator>
#include <random>
#include <unordered_map>
#include <utility>

namespace llarp
{
    static auto logcat = llarp::log::Cat("nodedb");

    static const std::filesystem::path RC_FILE_EXT{".signed"};

    std::array<int, 3> NodeDB::db_stats() const { return {num_rcs(), num_rids(), num_bootstraps()}; }

    const RemoteRC* NodeDB::get_random_rc(const std::function<bool(const RemoteRC&)>& predicate) const
    {
        const RemoteRC* result = nullptr;
        int admitted = 0;
        for (const auto& rc : std::views::values(known_rcs))
        {
            if (!predicate || predicate(rc))
            {
                if (admitted == 0 || std::uniform_int_distribution<int>{0, admitted}(llarp::csrng) == 0)
                    result = &rc;
                admitted++;
            }
        }
        return result;
    }

    std::vector<const RemoteRC*> NodeDB::get_n_random_rcs(
        int n, bool shuffle, const std::function<bool(const RemoteRC&)>& predicate) const
    {
        assert(_router.loop.inside());
        std::vector<const RemoteRC*> rand;
        rand.resize(n);
        auto all_rcs = known_rcs | std::views::values;
        auto to_ptr = std::views::transform([](const auto& rc) { return &rc; });
        auto end = predicate
            ? std::ranges::sample(all_rcs | std::views::filter(predicate) | to_ptr, rand.begin(), n, csrng)
            : std::ranges::sample(all_rcs | to_ptr, rand.begin(), n, csrng);
        if (auto len = std::distance(rand.begin(), end); len < n)
            rand.resize(len);
        if (shuffle && rand.size() > 1)
            std::ranges::shuffle(rand, csrng);
        return rand;
    }

    bool NodeDB::tick(std::chrono::milliseconds /*now*/)
    {
        assert(_router.loop.inside());
        if (_is_bootstrapping or _is_connecting_bstrap)
        {
            log::trace(logcat, "NodeDB deferring ::tick() to bootstrap fetch completion...");
            return false;
        }

        // TODO FIXME: there can be more than one bootstrap, and if we are a bootstrap then we may
        // still want to connect to other bootstraps, so this "just don't bootstrap if a seed" is
        // wrong.  We instead need something like "if we are the only bootstrap"

        // only enter bootstrap process if we have NOT marked initial fetch as needed
        if (_needs_bootstrap and not _router.config().bootstrap.seednode)
        {
            if (not _has_bstrap_connection)
            {
                if (_is_connecting_bstrap)
                {
                    log::trace(
                        logcat, "{} awaiting bstrap connect attempt...", _router.is_service_node ? "Relay" : "Client");
                    return false;
                }

                auto& brc = _bootstraps.current();
                auto bsrc = brc.router_id();

                log::critical(
                    logcat,
                    "{} has 0 router connections; connecting to bootstrap {}...",
                    _router.is_service_node ? "Relay" : "Client",
                    bsrc);

                log::critical(logcat, "BOOTSTRAPPING FIXME");
                // Temporarily disable this code to be fixed in the following commits
#if 0
                _router.link_manager().connect_to(
                    brc,
                    [this](quic::connection_interface& ci) {
                        log::info(logcat, "Successfully connected to bootstrap node!");
                        _has_bstrap_connection = true;
                        _is_connecting_bstrap = false;
                        return _router.link_manager().on_conn_open(ci);
                    },
                    [this](quic::connection_interface& ci, uint64_t ec) {
                        log::warning(logcat, "Failed to connect to bootstrap node!");
                        _is_connecting_bstrap = false;
                        return _router.link_manager().on_conn_closed(ci, ec);
                    });
#endif

                _is_connecting_bstrap = true;
                return false;
            }

            if (_bootstrap_handler and not _bootstrap_handler->is_iterating())
            {
                log::warning(
                    logcat,
                    "{} has {} of {} minimum RCs; initiating bootstrap RC fetch...",
                    _router.is_service_node ? "Relay" : "Client",
                    num_rcs(),
                    MIN_ACTIVE_RCS);
                _bootstrap_handler->start();
            }

            return false;
        }

        return true;
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

        remove_rcs_if([this, now](const RemoteRC& rc) -> bool {
            // don't purge bootstrap nodes from nodedb
            if (is_bootstrap_node(rc))
            {
                log::trace(logcat, "Not removing {}: is bootstrap node", rc.router_id());
                return false;
            }

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

            // clients have no notion of registered relays
            // we short circuit logic here so we dont remove
            // routers that are not registered for first hops
            if (not _router.is_service_node)
            {
                log::trace(logcat, "Not removing {}: we are a client and it looks fine", rc.router_id());
                return false;
            }

            // if we don't have the registered relay list yet don't remove the entry
            if (not has_registered_relays())
            {
                log::trace(
                    logcat, "Skipping check on {}: have not received oxend registered relay list yet", rc.router_id());
                return false;
            }

            if (not is_connection_allowed(rc.router_id()))
            {
                log::trace(logcat, "Removing {}: not a valid router", rc.router_id());
                return true;
            }

            return false;
        });

        _needs_bootstrap = num_rcs() < MIN_ACTIVE_RCS;
    }

    std::filesystem::path NodeDB::get_path_by_pubkey(const RouterID& pubkey) const
    {
        return _root / std::filesystem::path{pubkey.to_string()}.replace_extension(RC_FILE_EXT);
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

    bool NodeDB::is_bootstrap_node(const RemoteRC& rc) const
    {
        assert(_router.loop.inside());
        return _bootstraps.contains(rc.router_id());
    }

    void NodeDB::start_tickers()
    {
        log::trace(logcat, "NodeDB starting tickers...");

        // TODO FIXME: this startup pattern is very strange.  save_to_disk might fire before the
        // first purge_rcs, but why?  Wouldn't we be better with just *one* ticker here that does a
        // purge-then-save?

        _flush_ticker = _router.loop.call_every(FLUSH_INTERVAL, [this] { save_to_disk(); });
        _router.loop.call_later(uniform_duration_distribution{5s, 10s}(llarp::csrng), [this] { save_to_disk(); });

        _purge_ticker = _router.loop.call_every(
            PURGE_INTERVAL, [this] { purge_rcs(); }, not _needs_bootstrap);
        if (not _needs_bootstrap)
            _router.loop.call_later(uniform_duration_distribution{5s, 10s}(llarp::csrng), [this] { purge_rcs(); });

        if (not _router.is_service_node)
        {
            // start these immediately if we do not need to bootstrap
            _rc_fetch_ticker = _router.loop.call_every(
                FETCH_INTERVAL, [this] { fetch_rcs(); }, not _needs_bootstrap);

            _rid_fetch_ticker = _router.loop.call_every(
                FETCH_INTERVAL, [this] { fetch_rids(); }, not _needs_bootstrap);

            if (not _needs_bootstrap)
            {
                _router.loop.call_later(uniform_duration_distribution{10s, 15s}(llarp::csrng), [this] { fetch_rcs(); });
                _router.loop.call_later(uniform_duration_distribution{5s, 10s}(llarp::csrng), [this] { fetch_rids(); });
            }
        }
    }

    NodeDB::NodeDB(Router& r) : _router{r}, _root{_router.config().router.data_dir / nodedb_dirname}
    {
        if (not exists(_root))
            create_directory(_root);
        if (not is_directory(_root))
            throw std::runtime_error{fmt::format("nodedb {} is not a directory", _root)};

        auto seed = _router.config().bootstrap.seednode;
        if (seed)
            log::warning(logcat, "Local instance is bootstrap seed node!");

        _bootstraps.populate(
            _router.netid(),
            _router.config().bootstrap.files,
            _router.config().router.data_dir / default_bootstrap,
            not seed);

        bootstrap_init();
        load_from_disk();

        _needs_bootstrap = num_rcs() < MIN_ACTIVE_RCS;
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

    void NodeDB::stop_bootstrap(bool success)
    {
        _is_bootstrapping = false;
        // this function is only called in success or lokinet shutdown, so we will never need bootstrapping
        _needs_bootstrap = false;
        _bootstrap_handler->stop();

        if (success)
        {
            log::debug(
                logcat, "{} completed processing BootstrapRC fetch!", _router.is_service_node ? "Relay" : "Client");

            if (not _purge_ticker->is_running())
            {
                log::trace(logcat, "{} activating NodeDB purge ticker", _router.is_service_node ? "Relay" : "Client");
                _purge_ticker->start();
            }

            if (not _router.is_service_node)
            {
                if (not _rid_fetch_ticker->is_running())
                {
                    log::trace(logcat, "Client starting RID fetch ticker");
                    _rid_fetch_ticker->start();
                }

                if (not _rc_fetch_ticker->is_running())
                {
                    log::trace(logcat, "Client starting RC fetch ticker");
                    _rc_fetch_ticker->start();
                }
            }
        }
        else
            log::critical(
                logcat,
                "{} stopping bootstrap without a successful fetch!",
                _router.is_service_node ? "Relay" : "Client");
    }

    void NodeDB::bootstrap()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (_router.is_stopping() || not _router.is_running())
        {
            log::debug(logcat, "NodeDB unable to continue bootstrap fetch -- router is stopped!");
            return stop_bootstrap(false);
        }

        auto rc = _is_bootstrapping.exchange(true) ? _bootstraps.next() : _bootstraps.current();
        auto source = rc.router_id();

        log::debug(logcat, "Dispatching BootstrapRC to {}", source.short_string());

        auto num_needed = _router.is_service_node ? SERVICE_NODE_BOOTSTRAP_SOURCE_COUNT : CLIENT_BOOTSTRAP_SOURCE_COUNT;

        _router.link_endpoint().send_command(
            rc,
            "bfetch_rcs",
            BootstrapFetch::serialize(
                _router.is_service_node ? std::make_optional(_router.rc()) : std::nullopt, num_needed),
            [this, source](quic::message m) { handle_bootstrap_result(source, std::move(m)); });
    }

    void NodeDB::handle_bootstrap_result(const RouterID& source, quic::message m)
    {
        assert(_router.loop.inside());
        log::debug(logcat, "Received response to BootstrapRC fetch request...");

        if (not m)
        {
            log::warning(logcat, "BootstrapRC fetch request to {} failed", source.short_string());
            return;
        }

        int num = 0, accepted = 0;

        try
        {
            oxenc::bt_dict_consumer btdc{m.body()};

            btdc.required("r");

            {
                auto sublist = btdc.consume_list_consumer();

                while (not sublist.is_finished())
                {
                    // if we're trusting the bootstrap for RCs regardless of RouterID, we
                    // should trust the RouterID as well.
                    RemoteRC new_rc{sublist.consume_dict_data(), _router.netid()};
                    known_rids.insert(new_rc.router_id());
                    accepted += put_rc(std::move(new_rc));
                    ++num;
                }
            }
        }
        catch (const std::exception& e)
        {
            log::warning(
                logcat, "Failed to parse BootstrapRC fetch response from {}: {}", source.short_string(), e.what());
            return;
        }

        if (num >= MIN_ACTIVE_RCS)
        {
            log::info(
                logcat,
                "{} BootstrapRC fetch successfully produced {} RCs ({} minimum needed) with {} accepted",
                _router.is_service_node ? "Relay" : "Client",
                num,
                MIN_ACTIVE_RCS,
                accepted);
            return stop_bootstrap(true);
        }

        log::warning(
            logcat,
            "BootstrapRC response from {} returned {} RCs ({} minimum needed); continuing bootstrapping...",
            source.short_string(),
            num,
            MIN_ACTIVE_RCS);
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

    bool NodeDB::is_connection_allowed(const RouterID& remote) const
    {
        assert(_router.loop.inside());
        if (not _router.is_service_node)
        {
            if (_pinned_edges.size() and not _pinned_edges.contains(remote) and not _bootstraps.contains(remote))
                return false;

            return known_rids.contains(remote);
        }

        return known_rids.contains(remote) and is_registered(remote);
    }

    bool NodeDB::is_first_hop_allowed(const RouterID& remote) const
    {
        assert(_router.loop.inside());
        if (_pinned_edges.size() && _pinned_edges.count(remote) == 0)
            return false;

        return true;
    }

    void NodeDB::set_pinned_edges(std::unordered_set<RouterID> edges)
    {
        _strict_connect = true;
        _pinned_edges = std::move(edges);
    }

    void NodeDB::bootstrap_init()
    {
        log::trace(logcat, "NodeDB storing bootstraps...");

        if (_bootstraps.empty())
            return;

        size_t counter{0};

        for (size_t i = 0; i < _bootstraps.size(); i++)
            counter += put_rc(_bootstraps.next());

        auto success = counter == _bootstraps.size();
        log::log(
            logcat,
            success ? log::Level::info : log::Level::err,
            "NodeDB loaded {}/{} bootstrap routers",
            counter,
            _bootstraps.size());

        _bootstrap_handler = _router.loop.make_shared<EventTrigger>(
            _router.loop, FETCH_ATTEMPT_INTERVAL, [this]() { bootstrap(); }, FETCH_ATTEMPTS);
    }

    void NodeDB::load_from_disk()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (_root.empty())
            return;

        std::vector<std::filesystem::path> purge;

        const auto now = time_now_ms();

        for (const auto& f : std::filesystem::directory_iterator{_root})
        {
            if (not f.is_regular_file() or f.path().extension() != RC_FILE_EXT)
                continue;

            std::optional<RemoteRC> rc;
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
            known_rids.insert(rid);
            known_rcs.emplace(std::move(rid), std::move(*rc));
        }

        if (not purge.empty())
        {
            log::warning(logcat, "removing {} invalid RCs from disk", purge.size());
            for (const auto& fpath : purge)
                remove(fpath);
        }
    }

    void NodeDB::save_to_disk() const
    {
        // TODO FIXME: we should have a "changed" flag here so that we only write anything to disk
        // if it has changed.  Otherwise we're writing 2000 files to disk every iteration.

        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        assert(_router.loop.inside());

        if (_root.empty())
            return;

        // Copy the set of rcs to the disk loop to be processed as slowly as it wants:
        _router.disk_loop.call([this, known_rcs = known_rcs] {
            auto start = std::chrono::steady_clock::now();
            log::trace(logcat, "Writing NodeDB contents to disk...");

            for (const auto& [rid, rc] : known_rcs)
                rc.write(get_path_by_pubkey(rid));

            log::debug(
                logcat,
                "Wrote NodeDB contents to disk in {}",
                std::chrono::round<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start));
        });
    }

    void NodeDB::cleanup()
    {
        if (_bootstrap_handler)
        {
            log::trace(logcat, "NodeDB clearing bootstrap handler...");
            _bootstrap_handler->stop();
            _bootstrap_handler.reset();
        }

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

        if (_flush_ticker)
        {
            log::trace(logcat, "NodeDB clearing flush ticker...");
            _flush_ticker->stop();
            _flush_ticker.reset();
        }

        log::debug(logcat, "NodeDB cleared all tickers...");
    }

    const RemoteRC* NodeDB::get_rc(const RouterID& pk) const
    {
        assert(_router.loop.inside());
        auto it = known_rcs.find(pk);
        return it != known_rcs.end() ? &it->second : nullptr;
    }

    bool NodeDB::put_rc(const RemoteRC& rc)
    {
        assert(_router.loop.inside());

        if (rc.router_id() == _router.local_rid())
            return false;

        auto it = known_rcs.find(rc.router_id());
        if (it == known_rcs.end())
        {
            known_rcs.emplace(rc.router_id(), rc);
            return true;  // New RC hurray, gossip the good news!
        }

        auto& stored = it->second;
        if (!rc.newer_than(stored, RemoteRC::MIN_GOSSIP_RC_AGE))
            return false;

        // This RC is an update of one we already had: we only gossip if this RC indicates a changed
        // address (e.g. port or IP change) or was the first RC from this node in a long time, both
        // of which are updates we want to waste a little extra network bandwidth for to get out
        // everywhere ASAP via gossipping.
        bool significant = rc.newer_than(stored, RemoteRC::OUTDATED_AGE) || rc.address_changed(stored);

        stored = rc;

        return significant;
    }

    bool NodeDB::verify_store_gossip_rc(const RemoteRC& rc)
    {
        assert(_router.loop.inside());
        if (not is_registered(rc.router_id()))
            return false;
        return put_rc(rc);
    }

    int NodeDB::num_rcs() const
    {
        assert(_router.loop.inside());
        return static_cast<int>(known_rcs.size());
    }

    int NodeDB::num_rids() const
    {
        assert(_router.loop.inside());
        return static_cast<int>(known_rids.size());
    }

    void NodeDB::remove_rcs_if(const std::function<bool(const RemoteRC&)>& remove)
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
}  // namespace llarp
