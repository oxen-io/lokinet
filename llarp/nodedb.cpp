#include "nodedb.hpp"

#include "crypto/types.hpp"
#include "link/link_manager.hpp"
#include "messages/fetch.hpp"
#include "util/time.hpp"

#include <oxen/quic/btstream.hpp>

#include <algorithm>
#include <functional>
#include <iterator>
#include <unordered_map>
#include <utility>

namespace llarp
{
    static auto logcat = llarp::log::Cat("nodedb");

    static constexpr auto RC_FILE_EXT = ".signed"sv;

    std::tuple<size_t, size_t, size_t> NodeDB::db_stats() const { return {num_rcs(), num_rids(), num_bootstraps()}; }

    const RemoteRC* NodeDB::get_random_rc(const std::function<bool(const RemoteRC&)>& predicate) const
    {
        auto rcs = get_n_random_rcs(1, false, predicate);
        return rcs.empty() ? nullptr : &rcs[0].get();
    }

    std::vector<std::reference_wrapper<const RemoteRC>> NodeDB::get_n_random_rcs(
        int n, bool shuffle, const std::function<bool(const RemoteRC&)>& predicate) const
    {
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
        std::vector<std::reference_wrapper<const RemoteRC>> result;
        result.reserve(rand.size());
        for (auto* rc : rand)
            result.push_back(std::ref(*rc));
        return result;
    }

    bool NodeDB::tick(std::chrono::milliseconds /*now*/)
    {
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
                        logcat,
                        "{} awaiting bstrap connect attempt...",
                        _router.is_service_node ? "Relay" : "Client");
                    return false;
                }

                auto& brc = _bootstraps.current();
                auto bsrc = brc.router_id();

                log::critical(
                    logcat,
                    "{} has 0 router connections; connecting to bootstrap {}...",
                    _router.is_service_node ? "Relay" : "Client",
                    bsrc);

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
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (_router.is_stopping() || not _router.is_running())
        {
            log::debug(logcat, "NodeDB unable to continue purge ticking -- router is stopped!");
            return;
        }

        remove_rcs_if([&](const RemoteRC& rc) -> bool {
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

            // clients have no notion of a whilelist
            // we short circuit logic here so we dont remove
            // routers that are not whitelisted for first hops
            if (not _router.is_service_node)
            {
                log::trace(logcat, "Not removing {}: we are a client and it looks fine", rc.router_id());
                return false;
            }

            // if we don't have the whitelist yet don't remove the entry
            if (not _router.has_whitelist())
            {
                log::trace(logcat, "Skipping check on {}: don't have whitelist yet", rc.router_id());
                return false;
            }

            // if we have no whitelist enabled or we have
            // the whitelist enabled and we got the whitelist
            // check against the whitelist and remove if it's not
            // in the whitelist OR if there is no whitelist don't remove
            if (not is_connection_allowed(rc.router_id()))
            {
                log::trace(logcat, "Removing {}: not a valid router", rc.router_id());
                return true;
            }

            return false;
        });

        _needs_bootstrap = num_rcs() < MIN_ACTIVE_RCS;
    }

    fs::path NodeDB::get_path_by_pubkey(const RouterID& pubkey) const
    {
        return "{}/{}{}"_format(_root.native(), pubkey.to_string(), RC_FILE_EXT);
    }

    void NodeDB::process_fetched_rcs(std::vector<RemoteRC> rcs)
    {
        int accepted = 0;
        for (auto& rc : rcs)
        {
            auto& rid = rc.router_id();
            if (_router.is_service_node)
            {
                if (!_registered_routers.contains(rid))
                {
                    log::debug(logcat, "Rejecting fetched RC router {}: not found in registered router list", rid);
                    continue;
                }
            }
            else
            {
                if (!known_rids.contains(rc.router_id()))
                {
                    log::debug(
                        logcat,
                        "Fetched RC list contains {} RID {}; discarding it.",
                        unconfirmed_rids.contains(rc.router_id()) ? "unconfirmed" : "unknown",
                        rc.router_id());
                    continue;
                }
            }
            accepted++;
            put_rc(std::move(rc));
        }

        int rejected = static_cast<int>(rcs.size()) - accepted;
        double fetch_threshold = rcs.empty() ? 0.0 : accepted / (double)rcs.size();

        log::info(logcat, "RC fetch returned {} RCs ({} good, {} rejected)", rcs.size(), accepted, rejected);

        if (accepted < MIN_GOOD_RC_FETCH_TOTAL or fetch_threshold < MIN_GOOD_RC_FETCH_THRESHOLD)
        {
            log::warning(logcat, "RC acceptance rate is too low; reselecting RC fetch source");
            cycle_fetch_source();
        }
    }

    void NodeDB::process_unconfirmed_rids(std::set<RouterID> unconfirmed)
    {
        // before we add the unconfirmed set, we check to see if our local set of unconfirmed
        // rcs/rids appeared in the latest unconfirmed set; if so, we will increment their
        // number of verifications and reset the attempts counter. Once appearing in 3 different
        // requests, the rc/rid will be "verified" and promoted to the known_{rcs,rids}
        // container
        for (auto itr = unconfirmed_rids.begin(); itr != unconfirmed_rids.end();)
        {
            auto [id, votes] = *itr;
            if (auto found = unconfirmed.find(id); found != unconfirmed.end())
            {
                if (++votes >= CONFIRMATION_THRESHOLD)
                {
                    known_rids.insert(id);
                    itr = unconfirmed_rids.erase(itr);
                }
                else
                {
                    // Not enough votes yet, so leave it and continue.
                    ++itr;
                }
                unconfirmed.erase(found);
            }
            else if (--votes <= -CONFIRMATION_THRESHOLD)
                itr = unconfirmed_rids.erase(itr);
            else
                ++itr;
        }

        // Anything still left in `unconfirmed` wasn't found in unconfirmed_rids and so is a new
        // item we hadn't seen before; insert them into the unconfirmed map with an initial vote
        // count of 1 for this observation.
        for (const auto& id : unconfirmed)
            unconfirmed_rids[id] = 1;
    }

    /** We only call into this function after ensuring two conditions:
          1) We have received all 12 responses from the queried RouterID sources, whether that
            response was a timeout or not
          2) Of those responses, less than 4 were errors of any sorts

        Upon receiving each response from the rid fetch sources, the returned rid's are incremented
        in fetch_counters. This greatly simplifies the analysis required by this function to the
        determine success or failure:
          - If the frequency of each rid is above a threshold, it is accepted
          - If the number of accepted rids is below a certain amount, the set is rejected

        Logically, this function performs the following basic analysis of the returned RIDs:
          1) All responses are coalesced into a union set with no repetitions
          2) If we are bootstrapping:
              - The routerID's returned
    */
    void NodeDB::process_fetched_rids()
    {
        std::set<RouterID> union_set, confirmed_set, unconfirmed_set;

        for (const auto& [rid, count] : rid_result_counters)
        {
            log::trace(logcat, "RID: {}, Freq: {}", rid.short_string(), count);
            if (count >= MIN_RID_FETCH_FREQ)
                union_set.insert(rid);
            else
                unconfirmed_set.insert(rid);
        }

        // get the intersection of accepted rids and local rids
        std::set_intersection(
            known_rids.begin(),
            known_rids.end(),
            union_set.begin(),
            union_set.end(),
            std::inserter(confirmed_set, confirmed_set.begin()));

        // the total number of rids received
        const auto num_received = (double)(rid_result_counters.size());
        // the total number of received AND accepted rids
        const auto union_size = union_set.size();

        const auto fetch_threshold = (double)union_size / num_received;

        bool success = (fetch_threshold >= GOOD_RID_FETCH_THRESHOLD) and (union_size >= MIN_GOOD_RID_FETCH_TOTAL);

        log::trace(
            logcat,
            "Num received: {}, union size: {}, known rid size: {}, fetch_threshold: {}, status: {}",
            num_received,
            union_size,
            known_rids.size(),
            fetch_threshold,
            success ? "SUCCESS" : "FAIL");

        /** We are checking 2 things here:
            1) The ratio of received/accepted to total received is above GOOD_RID_FETCH_THRESHOLD.
            This tells us how well the rid source's sets of rids "agree" with one another
            2) The total number received is above MIN_RID_FETCH_TOTAL. This ensures that we are
            receiving a sufficient amount to make a comparison of any sorts
        */
        if (success)
        {
            log::info(logcat, "RID fetch was successful: accumulated RID's accepted by trust model");
            process_unconfirmed_rids(std::move(unconfirmed_set));
            known_rids.merge(confirmed_set);
            post_rid_fetch(false);
        }
        else
        {
            log::warning(logcat, "Accumulated RID's rejected by trust model; reselecting RID fetch sources...");
            reselect_router_id_sources(fail_sources);
        }
    }

    std::vector<RouterID> NodeDB::get_expired_rcs()
    {
        auto expired = known_rcs | std::views::filter([](const auto& id_rc) { return id_rc.second.is_outdated(); })
            | std::views::keys;
        return {expired.begin(), expired.end()};
    }

    void NodeDB::fetch_rcs()
    {
        if (_router.is_stopping() || not _router.is_running())
        {
            log::debug(logcat, "NodeDB unable to continue RC fetch -- router is stopped!");
            if (_rc_fetch_ticker)
                _rc_fetch_ticker->stop();
            return;
        }

        cycle_fetch_source();

        log::debug(logcat, "Dispatching FetchRC's request to {}!", fetch_source.short_string());

        _router.link_manager().fetch_rcs(
            fetch_source,
            FetchRC::serialize(get_expired_rcs()),
            [this, source = fetch_source](quic::message m) mutable {
                std::string error;
                if (m)
                    try
                    {
                        auto rcs = FetchRC::deserialize_response(_router.netid(), oxenc::bt_dict_consumer{m.body()});
                        log::trace(logcat, "RC fetching was successful; processing {} returned RCs...", rcs.size());
                        return process_fetched_rcs(std::move(rcs));
                    }
                    catch (const std::exception& e)
                    {
                        error = e.what();
                    }
                else
                    error = m.timed_out ? "timed out" : "failed: {}"_format(m.body());

                log::warning(logcat, "RC fetch from {} failed: {}; reselecting RC fetch source", source, error);
                cycle_fetch_source();
            });
    }

    // FIXME: all of this RouterID and RC fetching code is pretty nasty and jank,
    //        but more importantly fragile and not working.
    void NodeDB::fetch_rids()
    {
        if (_router.is_stopping() || not _router.is_running())
        {
            log::debug(logcat, "NodeDB unable to continue RouterID fetch -- router is stopped!");
            // FIXME: this *was* calling post_rid_fetch, but that seems wrong (and can segfault),
            //        might need to see *why* it was doing so, if for any logical reason
            return;
        }

        if (rid_sources.empty())
        {
            log::debug(logcat, "Client reselecting RID sources...");
            reselect_router_id_sources(rid_sources);
        }

        fetch_counter = 0;
        response_counter = 0;
        fail_counter = 0;
        fail_sources.clear();
        rid_result_counters.clear();

        do
            cycle_fetch_source();
        while (rid_sources.contains(fetch_source));

        auto& src = fetch_source;
        log::debug(logcat, "New fetch source is {}", src);

        auto send_hook = [this, src = src](quic::BTRequestStream& control) mutable {
            for (const RouterID& target : rid_sources)
            {
                if (target == src)
                    return;

                log::trace(logcat, "Sending FetchRIDs request to {} via {}", target, src);
                control.command(
                    "fetch_rids",
                    FetchRID::serialize(target),
                    [&, source = src, target = target](quic::message msg) mutable {
                        // Since we batch send this without going through link_manager, wrap the response handler
                        // in loop-call from here
                        _router.loop.call([&, m = std::move(msg)]() mutable {
                            response_counter++;
                            if (not m)
                            {
                                log::warning(
                                    logcat,
                                    "RID fetch from {} via {} {}",
                                    target,
                                    source,
                                    m.timed_out ? "timed out" : "failed: {}"_format(m.body()));
                                ingest_fetched_rids(source);
                            }
                            else
                            {
                                try
                                {
                                    std::unordered_set<RouterID> router_ids;
                                    oxenc::bt_dict_consumer btdc{m.body()};

                                    btdc.required("r");

                                    {
                                        auto sublist = btdc.consume_list_consumer();

                                        while (not sublist.is_finished())
                                            router_ids.emplace(sublist.consume_span<uint8_t, 32>());
                                    }

                                    btdc.require_signature(
                                        "~", [&target](std::span<const std::byte> msg, std::span<const std::byte> sig) {
                                            if (sig.size() != SIGSIZE)
                                                throw std::runtime_error{"Invalid signature: not 64 bytes"};
                                            if (not crypto::verify(target, msg, sig.first<SIGSIZE>()))
                                                throw std::runtime_error{
                                                    "Failed to verify signature for fetch RouterIDs response."};
                                        });

                                    ingest_fetched_rids(source, std::move(router_ids));
                                }
                                catch (const std::exception& e)
                                {
                                    log::warning(logcat, "Error handling fetch RouterIDs response: {}", e.what());
                                    ingest_fetched_rids(source);
                                }
                            }
                        });
                    });

                fetch_counter++;
            }
        };

        _router.link_manager().fetch_router_ids(src, std::move(send_hook));
    }

    void NodeDB::ingest_fetched_rids(const RouterID& source, std::optional<std::unordered_set<RouterID>> rids)
    {
        log::trace(logcat, "Ingesting {} RID's from {}", rids ? rids->size() : 0, source);

        if (rids)
        {
            for (const auto& rid : *rids)
                rid_result_counters[rid]++;
        }
        else
        {
            fail_sources.insert(source);
            fail_counter++;
            log::trace(logcat, "{} marked as a failed fetch source (currently: {})", source, fail_counter);
        }

        int n_fails = fail_counter.load();
        int n_responses = response_counter.load();

        if (n_responses < fetch_counter)
        {
            log::trace(logcat, "Received {}/{} fetch RID requests", n_responses, fetch_counter);
            return;
        }

        log::trace(logcat, "Received {}/{} fetch RID requests! Processing...", n_responses, fetch_counter);

        if (n_fails <= MAX_RID_ERRORS)
        {
            log::debug(logcat, "RID fetching was successful ({}/{} acceptable errors)", n_fails, MAX_RID_ERRORS);
            return process_fetched_rids();
        }

        log::warning(logcat, "RID fetching found {} failures; reselecting failed RID fetch sources...", n_fails);
        reselect_router_id_sources(fail_sources);
    }

    bool NodeDB::is_bootstrap_node(const RemoteRC& rc) const { return _bootstraps.contains(rc.router_id()); }

    void NodeDB::start_tickers()
    {
        log::trace(logcat, "NodeDB starting tickers...");

        // TODO FIXME: this startup pattern is very strange.  save_to_disk might fire before the
        // first purge_rcs, but why?  Wouldn't we be better with just *one* ticker here that does a
        // purge-then-save?

        _flush_ticker = _router.loop.call_every(FLUSH_INTERVAL, [this] { save_to_disk(); });
        _router.loop.call_later(uniform_duration_distribution{5s, 10s}(llarp::csrng), [this] { save_to_disk(); });

        _purge_ticker = _router.loop.call_every(PURGE_INTERVAL, [this] { purge_rcs(); }, not _needs_bootstrap);
        if (not _needs_bootstrap)
            _router.loop.call_later(uniform_duration_distribution{5s, 10s}(llarp::csrng), [this] { purge_rcs(); });

        if (not _router.is_service_node)
        {
            // start these immediately if we do not need to bootstrap
            _rc_fetch_ticker =
                _router.loop.call_every(FETCH_INTERVAL, [this] { fetch_rcs(); }, not _needs_bootstrap);

            _rid_fetch_ticker =
                _router.loop.call_every(FETCH_INTERVAL, [this] { fetch_rids(); }, not _needs_bootstrap);

            if (not _needs_bootstrap)
            {
                _router.loop.call_later(
                    uniform_duration_distribution{5s, 10s}(llarp::csrng), [this] { fetch_rcs(); });
                _router.loop.call_later(
                    uniform_duration_distribution{5s, 10s}(llarp::csrng), [this] { fetch_rids(); });
            }
        }
    }

    NodeDB::NodeDB(Router& r) : _router{r}, _root{_router.config().router.data_dir / nodedb_dirname}
    {
        if (not fs::exists(_root))
            fs::create_directory(_root);
        if (not fs::is_directory(_root))
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

        auto num_needed =
            _router.is_service_node ? SERVICE_NODE_BOOTSTRAP_SOURCE_COUNT : CLIENT_BOOTSTRAP_SOURCE_COUNT;

        _router.link_manager().fetch_bootstrap_rcs(
            rc,
            BootstrapFetch::serialize(
                _router.is_service_node ? std::make_optional(_router.rc()) : std::nullopt, num_needed),
            [this, source](quic::message m) {
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
                        logcat,
                        "Failed to parse BootstrapRC fetch response from {}: {}",
                        source.short_string(),
                        e.what());
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
            });
    }

    // Updates `current` to not contain any of the elements of `replace` and resamples (up to
    // `target_size`) from population to refill it.
    template <typename T, typename RNG>
    static void replace_subset(
        std::unordered_set<T>& current,
        const std::unordered_set<T>& replace,
        std::set<T> population,
        size_t target_size,
        RNG&& rng)
    {
        for (auto it = replace.begin(); it != replace.end(); ++it)
        {
            // Remove the ones we are replacing from current:
            current.erase(*it);
            // Remove from the population to not reselect
            population.erase(*it);
        }

        for (auto it = current.begin(); it != current.end(); ++it)
            population.erase(*it);

        if (current.size() < target_size)
            std::sample(
                population.begin(),
                population.end(),
                std::inserter(current, current.end()),
                target_size - current.size(),
                rng);
    }

    void NodeDB::reselect_router_id_sources(std::unordered_set<RouterID> specific)
    {
        replace_subset(rid_sources, specific, known_rids, RID_SOURCE_COUNT, csrng);

        if (auto sz = rid_sources.size(); sz < RID_SOURCE_COUNT)
        {
            log::warning(logcat, "Insufficient RID's (count: {}) held locally for fetching!", sz);
        }
    }

    void NodeDB::set_router_whitelist(const std::vector<RouterID>& whitelist)
    {
        log::debug(logcat, "Oxend provided {} whitelisted routers", whitelist.size());

        if (whitelist.empty())
            return;

        _registered_routers.clear();
        _registered_routers.insert(whitelist.begin(), whitelist.end());

        log::info(
            logcat, "Service node holding {} registered relays after oxend integration", _registered_routers.size());
    }

    std::optional<RouterID> NodeDB::get_random_registered_router() const
    {
        auto result = std::make_optional<RouterID>();
        std::function<bool(RouterID)> hook = [](const auto&) -> bool { return true; };
        auto end = std::ranges::sample(_registered_routers, &*result, 1, llarp::csrng);
        if (end == &*result)
            result.reset();
        return result;
    }

    bool NodeDB::is_connection_allowed(const RouterID& remote) const
    {
        if (not _router.is_service_node)
        {
            if (_pinned_edges.size() && _pinned_edges.count(remote) == 0 && not _bootstraps.contains(remote))
                return false;

            return known_rids.count(remote);
        }

        return known_rids.count(remote) and _registered_routers.empty() ? true : _registered_routers.count(remote);
    }

    bool NodeDB::is_first_hop_allowed(const RouterID& remote) const
    {
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
        Lock_t l{nodedb_mutex};

        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (_root.empty())
            return;

        std::vector<fs::path> purge;

        const auto now = time_now_ms();

        for (const auto& f : fs::directory_iterator{_root})
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
                fs::remove(fpath);
        }
    }

    void NodeDB::save_to_disk() const
    {
        // TODO FIXME: we should have a "changed" flag here so that we only write anything to disk
        // if it has changed.  Otherwise we're writing 2000+ files to disk every few seconds.

        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (_root.empty())
            return;

        log::trace(logcat, "Writing NodeDB contents to disk...");

        for (const auto& [rid, rc] : known_rcs)
            rc.write(get_path_by_pubkey(rid));

        log::trace(logcat, "Done writing NodeDB contents");
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
        auto it = known_rcs.find(pk);
        return it != known_rcs.end() ? &it->second : nullptr;
    }

    bool NodeDB::put_rc(RemoteRC rc)
    {
        Lock_t l{nodedb_mutex};

        const auto& rid = rc.router_id();

        if (rid == _router.local_rid())
            return false;

        auto [it, inserted] = known_rcs.try_emplace(rc.router_id(), std::move(rc));
        if (inserted)
            return true;
        if (it->second.other_is_newer(rc))
        {
            it->second = std::move(rc);
            return true;
        }
        return false;
    }

    size_t NodeDB::num_rcs() const { return known_rcs.size(); }

    size_t NodeDB::num_rids() const { return known_rids.size(); }

    void NodeDB::cycle_fetch_source()
    {
        if (known_rids.empty())
            return fetch_source.zero();

        fetch_source = *std::next(
            known_rids.begin(),
            std::uniform_int_distribution{0, static_cast<int>(known_rids.size()) - 1}(llarp::csrng));

        log::debug(logcat, "Updated RC fetch source to {}", fetch_source);
    }

    void NodeDB::remove_rcs_if(const std::function<bool(const RemoteRC&)>& remove)
    {
        // only called from within event loop ticker
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

    bool NodeDB::verify_store_gossip_rc(const RemoteRC& rc)
    {
        return registered_routers().contains(rc.router_id()) && put_rc(rc);
    }

    void NodeDB::remove_many_from_disk_async(const std::vector<RouterID>& remove) const
    {
        if (_root.empty())
            return;

        // build file list
        std::vector<fs::path> files;
        files.reserve(remove.size());
        for (const auto& rid : remove)
            files.push_back(get_path_by_pubkey(rid));

        // remove them from the disk via the diskio thread
        _router.disk_loop.call_soon([files = std::move(files)] {
            for (const auto& p : files)
                fs::remove(p);
        });
    }

    std::vector<const RemoteRC*> NodeDB::find_many_closest_to(llarp::hash_key location, int num_routers) const
    {
        if (num_routers <= 0)
            return {};

        std::vector<const RemoteRC*> rcs;
        rcs.reserve(known_rcs.size());
        for (const auto& [id, rc] : known_rcs)
            rcs.push_back(&rc);
        if (num_routers >= static_cast<int>(rcs.size()))
            return rcs;

        std::ranges::nth_element(
            rcs, rcs.begin() + num_routers, XorMetric{location}, [](const auto* a) -> auto& { return *a; });
        rcs.resize(num_routers);
        rcs.shrink_to_fit();
        return rcs;
    }
}  // namespace llarp
