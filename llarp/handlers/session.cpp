#include "session.hpp"

#include <llarp/constants/path.hpp>
#include <llarp/contact/contactdb.hpp>
#include <llarp/contact/relay_contact.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/link/endpoint.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/messages/fetch.hpp>
#include <llarp/messages/path.hpp>
#include <llarp/messages/session.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/path/path.hpp>
#include <llarp/path/transit_hop.hpp>
#include <llarp/router/router.hpp>
#include <llarp/session/session.hpp>
#include <llarp/util/bspan.hpp>
#include <llarp/util/random.hpp>
#include <llarp/util/time.hpp>

#include <oxenc/base32z.h>

#include <memory>
#include <random>

namespace llarp::handlers
{
    static auto logcat = log::Cat("session_ep");

    SessionEndpoint::SessionEndpoint(Router& r)
        : path::
              PathHandler{r, r.config().paths.inbound_paths + r.config().paths.inbound_paths_extra, r.config().paths.inbound_hops()},
          cc_blind_keys{r.secret_key(), crypto::blinding::CLIENT_CONTACT}
    {
        const auto& netconf = router.config().network;

        _auth_tokens = netconf.exit_auths;

        // *All* clients currently support speaking via QUIC tunnel:
        protocols = protocol_flag::QUIC_TUNNEL;
        if (!router.embedded())
        {
            // raw IPv4/IPv6/exit traffic all require a full tun interface.

            protocols = protocol_flag::IPV4;
            if (netconf.enable_ipv6)
                protocols |= protocol_flag::IPV6;
            if (router.is_exit_node())
                protocols |= protocol_flag::EXIT;
        }

        client_contact =
            ClientContact{router.key_manager.router_id(), netconf.srv_records, protocols, netconf.traffic_policy};
    }

    std::array<int, 5> SessionEndpoint::session_stats() const
    {
        std::array<int, 5> stats{0};
        auto& [in, out_r, out_c, out_r_pending, out_c_pending] = stats;

        for (const auto& s : std::views::values(_sessions))
        {
            if (s->is_closed())
                continue;
            if (s->is_outbound)
            {
                if (s->is_relay_session)
                {
                    out_r++;
                    if (!s->is_established())
                        out_r_pending++;
                }
                else
                {
                    out_c++;
                    if (!s->is_established())
                        out_c_pending++;
                }
            }
            else
                in++;
        }

        return stats;
    }

    std::array<int, 3> SessionEndpoint::path_stats(std::chrono::milliseconds now) const
    {
        std::array<int, 3> stats{0};
        auto& [in, out_r, out_c] = stats;
        in = num_paths();

        for (const auto& s : std::views::values(_sessions))
            if (!s->is_closed() && s->is_outbound)
            {
                auto& os = static_cast<const session::OutboundSession&>(*s);
                if (os.is_relay_session)
                    out_r += os.num_paths(now);
                else
                    out_c += os.num_paths(now);
            }

        return stats;
    }

    void SessionEndpoint::close_session(std::shared_ptr<session::Session>& s, bool send_close)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (!s)
            return;

        s->close(send_close);

        const auto& remote = s->remote();
        if (auto& tun = router.tun_endpoint())
            tun->unmap(remote);

        if (auto it = _sessions.find(remote); it != _sessions.end())
        {
            if (auto& s = it->second)
                _session_tags.erase(s->inbound_tag());
            _sessions.erase(it);
        }
    }

    bool SessionEndpoint::close_session(NetworkAddress remote, bool send_close)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (auto it = _sessions.find(remote); it != _sessions.end())
        {
            close_session(it->second, send_close);
            return true;
        }

        log::warning(logcat, "Could not find session (remote:{}) to close!", remote);
        return false;
    }

    bool SessionEndpoint::close_session(session_tag t, bool send_close)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (auto it = _session_tags.find(t); it != _session_tags.end())
        {
            close_session(it->second, send_close);
            return true;
        }

        log::warning(logcat, "Could not find session (tag:{}) to close!", t);
        return false;
    }

    void SessionEndpoint::tick(std::chrono::milliseconds now)
    {
        log::trace(logcat, "SessionEndpoint ticking sessions...");
        for (const auto& [addr, session] : _sessions)
            session->tick(now);

        path::PathHandler::tick(now);
    }

    void SessionEndpoint::stop(bool send_close)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        _running = false;

        if (_path_rotater)
        {
            _path_rotater.reset();
            log::trace(logcat, "Path rotation ticker stopped!");
        }

        // Do a best-effort close; if send_close is true these close(true) calls should queue a
        // path_close on the active stream, even though we immediately drop the streams below, which
        // should still typically arrive at the other side.
        for (auto& s : std::views::values(_sessions))
            s->close(send_close);

        _sessions.clear();
        _session_tags.clear();

        path::PathHandler::stop();
    }

    void SessionEndpoint::cleanup_old_fuzz(int oldest_slot)
    {
        if (auto it = _slot_fuzz.lower_bound(oldest_slot); it != _slot_fuzz.end() && it != _slot_fuzz.begin())
            _slot_fuzz.erase(_slot_fuzz.begin(), it);
    }

    std::chrono::seconds SessionEndpoint::inbound_path_fuzz(int slot)
    {
        auto it = _slot_fuzz.lower_bound(slot);
        if (it != _slot_fuzz.end() && it->first == slot)
            return it->second;

        // Note that this fuzz must not be negative!  If we allowed negative fuzz then a path could
        // expire *before* its slot expired, and as a result we would try to build a new very short
        // path to make up for the expired slot.
        std::normal_distribution<float> dist{0, path::MAX_LIFETIME_FUZZ.count() / 2.575829f};
        std::chrono::seconds fuzz;
        do
        {
            fuzz = std::chrono::seconds{static_cast<int>(dist(csrng))};
            if (fuzz < 0s)
                fuzz = -fuzz;
        } while (fuzz > path::MAX_LIFETIME_FUZZ);

        _slot_fuzz.emplace_hint(it, slot, fuzz);

        return fuzz;
    }

    void SessionEndpoint::update_paths(std::chrono::milliseconds now)
    {
        int have = num_paths(now);
        int needed = _target_paths - have;
        if (needed <= 0)
        {
            log::trace(
                logcat,
                "SessionEndpoint doesn't need more paths right now (have {} >= target {})",
                have,
                _target_paths);
            return;
        }

        if (cooldown())
        {
            log::debug(
                logcat,
                "SessionEndpoint needs {} more paths (to reach target {}), but path builds are currently in cooldown "
                "because the last {} path builds failed",
                needed,
                _target_paths,
                _consecutive_failures);
            return;
        }

        log::debug(
            logcat,
            "SessionEndpoint building {} additional paths to random remotes to reach target of {} paths",
            needed,
            _target_paths);

        // If we *don't* have distinct IP ranges from our current edges (e.g. by random chance, or
        // with only a single edge, or just because that's how the pinned edges pan out) then we
        // want to exclude the random terminus that we choose to exclude that singleton range from
        // being the terminus: because otherwise we can end up in a situation where it is impossible
        // to respect the distinct-ip-range setting because once we have selected a pivot, and feed
        // it into PathHandler::select_hops_to_remote, it has no choice but to use that pivot *and*
        // the edge, and those conflict.
        //
        // (If we have multiple ranges for edges then it's not an issue because whichever pivot we
        // select will have at least one available edge not in its range).
        //
        // So, if we're in that only-one-edge-ip-range case, we apply the edge exclusion back here at
        // terminus selection so that our selection here doesn't force select_hops_to_remote into
        // that situation.

        auto unique_edge_range = router.link_endpoint().unique_edge_range();

        auto filter = [this, &unique_edge_range](const RelayContact& rc) {
            if (unique_edge_range and unique_edge_range->contains(rc.addr().to_ipv4()))
                return false;

            // Exclude any inbound pivots we are already using so that we diversify:
            const auto& rid = rc.router_id();
            for (const auto& p : paths())
                if (p.terminal_rid() == rid)
                    return false;

            return not router.router_profiling().is_bad_for_path(rid, 1);
        };

        // Path lifetime selection
        // -----------------------
        //
        // We want our CC path expiries to be spread out temporally because if we have them
        // clustered together, we can end up in a situation where all our inbound paths expire at
        // the same time, and so someone connected to us cannot stay connected, because they have
        // no other paths to rotate to.
        //
        // To solve this, we try to ensure that paths are always spread out across expiries.  For
        // example, with 4 target inbound paths, we ideally want paths that expire at
        //
        // [P1: t₀+5min, P2: t₀+10min, P3: t₀+15min, P4: t₀+20min]
        //
        // and when we reach t₁ = t₀+5min, the first expires, and we build a new one for t₁+20min
        // (== t₀ + 25min), thus ending up with:
        //
        // [P2: t₁+5min, P3: t₁+10min, P4: t₁+15min, P5: t₁+20min]
        //
        // and so on over time.
        //
        // The problem, however, is that paths can die.  Suppose, for instance, that P4 dies at
        // t₂ = t₁+1min.  That means just before processing the path death we have:
        //
        // [P2: t₂+4min, P3: t₂+9min, P4: t₂+14min, P5: t₂+19min]
        //
        // and then after processing we have:
        //
        // [P2: t₂+4min, P3: t₂+9min, P5: t₂+19min]
        //
        // If we construct a new, full-lifetime path at this point we'll have:
        //
        // [P2: t₂+4min, P3: t₂+9min, P5: t₂+19min, P6: t₂+20min]
        //
        // which is okay for now, but will lead to us having a perpetual 1min gap between P5 and P6
        // (and then also between the P9 and P10 replacements when P5/6 expire), and a 9min gap
        // between P4/P5 (and between its replacements).
        //
        // Worse, if all your paths die at once (for instance, because your local internet died
        // temporarily) you would rebuild all 4 paths at once, and could end up with all expiring at
        // the same time.
        //
        // So it isn't enough to just spread them out initially: we have to create new paths with a
        // lifetime that slots them into the expiry time the path they are replacing would have had.
        // E.g. rather than the above lumpy distribution, we want the paths with P6 to look like:
        //
        // [P2: t₂+4min, P3: t₂+9min, P6: t₂+14min, P5: t₂+19min]
        //
        // so that "re-slotting" the path with the earlier timestamp keeps the distribution nice and
        // spread out, preventing unwanted clustering of expiries.
        //
        // For 2 or 3 paths, we space things out proportionally, e.g.:
        //
        // 2: [P1: t+10m, P2: t+20m]
        // 3: [P1: t+6m40s, P2: t+13m20s, P3: t+20m]
        //
        // Beyond 4, we use the same 5m spacing as with 4, but double up some slots.  For example,
        // with 7 paths:
        //
        // 7: [P1: t+5m, P2&P5: t+10m, P3&P6: t+15m, P4&P7: t+20m]
        //
        // This is somewhat lopsided for non-multiples of 4, but there's still lots of spread in
        // there so that even with multiple paths expiring at the same time, there are still lots of
        // alternatives for remotes to switch to.
        //
        // Note that all of the above ignores "fuzz", i.e. each path has a small random amount of
        // lifetime (well less than 5min) added to it to reduce the fingerprintability of path build
        // expiries.  All of the above still holds with respect to slots, it's just that where we
        // write "+Nm" it's actually "+Nm+fuzz[0,3m]".

        std::vector<std::chrono::seconds> expiries;
        expiries.reserve(needed);
        {
            const int slots = std::min(_target_paths, path::MAX_LIFETIME_SLOTS);

            // Our expiry slot size.  Generally 5min, but longer if you use fewer than 4 paths:
            const std::chrono::seconds slot_size = path::MAX_LIFETIME / slots;
            assert(path::MAX_LIFETIME % slots == 0s);

            std::array<int, path::MAX_LIFETIME_SLOTS> slot_count = {0};

            // The base slot, measured in multiples of `slot_size` relative to our fixed basis: we
            // consider other path expiries relative to this base slot.
            //
            // The +1 here is because (now-basis)/slot_size (i.e. without the +1) is going to give
            // us a slot index that translates to a slot start time in the past (i.e. 0-5min ago),
            // but we don't build for that slot: instead we build for slots at +5m, +10m, +15m, +20m
            // from that now-or-earlier point.  Thus +1 brings us up to the first slot position
            // within the next [0-5min], and that is our "slot0" value, i.e. the index 0 slot of all
            // slots we consider building for.
            //
            // There is an argument to be made to not build new paths that would only have a
            // duration of 0-5 min, but for now it's much simpler and cleaner to just build those
            // paths anyway (if no path in that slot).
            auto slot0 = (std::chrono::floor<std::chrono::seconds>(now) - path_expiry_basis) / slot_size + 1;

            // First count up all the slots we are already using with existing paths:
            int path_count = 0;
            for (auto& path : paths())
            {
                path_count++;
                // Path expiries will be up to +MAX_LIFETIME_FUZZ of their slot target expiry time, so we need
                // to be sure that the maximum fuzz is less then the smallest possible slot size so
                // that it is guaranteed to be counted in the same slot:
                static_assert(
                    path::MAX_LIFETIME_FUZZ < path::MAX_LIFETIME / path::MAX_LIFETIME_SLOTS,
                    "The slot calculation below requires path max fuzz be strictly smaller than the smallest allowed "
                    "path slot size!");
                auto slot = (path.expiry() - path_expiry_basis) / slot_size;
                if (slot < slot0)
                {
                    log::debug(logcat, "Ignoring expired/expiring path slot {}", slot);
                    continue;  // Path is expired/expiring, so ignore it.
                }
                slot -= slot0;
                if (slot >= slots)
                {
                    log::warning(logcat, "Found inbound path with unexpected future expiry, this should not happen!");
                    continue;
                }
                slot_count[slot]++;
            }

            log::trace(
                logcat, "Current {} path expiry slots (oldest-newest): {}", path_count, fmt::join(slot_count, "-"));

            // We want all paths built in a given slot to expire at the same time so that we publish
            // CCs on average once every 5 minutes, even if we are using many paths, and so we reuse
            // the same fuzz value for any paths built in the same slot (whether in this build or a
            // previous one that we are rebuilding for here).
            cleanup_old_fuzz(slot0);

            // Now we select new ones by looking for the slot with the fewest paths in it, preferring
            // later slots (i.e. longer expiries) in case of a tie, and keep repeating this for
            // however many paths we need:
            for (int i = 0; i < needed; i++)
            {
                int best = 0;
                for (int j = 1; j < slots; j++)
                    if (slot_count[j] <= slot_count[best])
                        best = j;
                const auto slot = slot0 + best;
                expiries.emplace_back(path_expiry_basis + slot * slot_size + inbound_path_fuzz(slot));
                slot_count[best]++;
            }

            log::trace(logcat, "Select new path expiries: {}", fmt::join(expiries, ", "));
        }

        auto next_expiry = expiries.begin();

        if (num_hops() == 1)
        {
            // In single-hop mode the edge and pivot are the same thing, and so we need to select
            // the edge (according to various configured edge selection rules), because if we
            // selected a random pivot first we'd bypass all the edge rules (pinned edges,
            // relay-connections, and so on).
            for (; needed > 0; needed--)
            {
                auto only_hop = select_first_hop();
                if (!only_hop)
                {
                    log::warning(logcat, "Unable to build a new inbound single-hop path: no eligible edges");
                    return;
                }
                build(std::span{&*only_hop, 1}, *next_expiry++);
            }
        }
        else
        {
            auto new_pivots = router.node_db().get_n_random_rcs(needed, true, filter);
            if (needed > static_cast<int>(new_pivots.size()))
                log::warning(
                    logcat,
                    "Unable to build {} new inbound paths: {} unused/acceptable pivots currently available",
                    needed,
                    new_pivots.size());
            for (const llarp::RelayContact* rc : new_pivots)
            {
                log::debug(logcat, "Selected new inbound path terminus {}", rc->router_id().short_string());
                auto hops = select_hops_to_remote(rc->router_id());
                if (!hops)
                    continue;  // No need to warn: the call above should already if it fails

                build(*hops, *next_expiry++);
            }
        }
    }

    void SessionEndpoint::on_path_build_success(int64_t /*build_id*/, path::Path& p)
    {
        log::debug(logcat, "Successfully built path {}", p);

        if (not router.config().network.is_reachable)
            return;

        // else we publish an introset, so check if we need to publish it now.

        // We just built a new path: if that path brings our active paths to the target number of
        // paths then we want to publish the introset.  *Typically* this will just end up on a
        // regular timer (i.e. paths expire naturally, we rebuild them, and once all expired ones
        // are rebuilt we end up here to republish), but in case of premature path death we might
        // also end up here (and also need to republish so that clients in the wild don't try
        // aligning to the dead path).

        if (num_active_paths() < _target_paths)
            return;  // We haven't met our target yet (or they are still building), so wait for them
                     // to finish rebuilding before we publish.

        log::info(logcat, "Inbound active paths changed; re-publishing client contact");
        update_and_publish_localcc();
    }

    void SessionEndpoint::on_path_build_failure(int64_t /*build_id*/, path::Path* path, bool timeout)
    {
        if (path)
            log::warning(logcat, "Path build for {} {}", *path, timeout ? "timed out" : "failed");
        else
            log::warning(logcat, "Path build failed: cannot construct a new path right now");
    }

    void SessionEndpoint::resolve_sns_mappings()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        auto& sns_ranges = router.config().exit.sns_ranges;

        if (not sns_ranges.empty())
        {
            log::debug(logcat, "SessionEndpoint resolving {} SNS addresses mapped to IP ranges", sns_ranges.size());

            for (const auto& [name, ip_range] : sns_ranges)
            {
                resolve_sns(name, [this, ip_range](std::optional<NetworkAddress> maybe_addr) {
                    if (maybe_addr)
                    {
                        log::critical(
                            logcat,
                            "UNIMPLEMENTED: Successfully resolved SNS lookup for {} mapped to IPRange:{}",
                            *maybe_addr,
                            ip_range);
                        // TODO FIXME: we need to sort out how these addresses get actually
                        // mapped.
                        //_range_map.insert_or_assign(std::move(ip_range), std::move(*maybe_addr));
                    }
                    // we don't need to print a fail message, as it is logged prior to invoking with std::nullopt
                });
            }
        }

        auto& sns_auths = router.config().network.sns_exit_auths;

        if (auto n_sns_auths = sns_auths.size(); n_sns_auths > 0)
        {
            log::debug(logcat, "SessionEndpoint resolving {} ONS addresses mapped to auth tokens", n_sns_auths);

            for (const auto& [name, auth_token] : sns_auths)
            {
                resolve_sns(name, [this, auth_token](std::optional<NetworkAddress> maybe_addr) {
                    if (maybe_addr)
                    {
                        log::debug(
                            logcat, "Successfully resolved SNS lookup for {} mapped to static auth token", *maybe_addr);
                        _auth_tokens.emplace(std::move(*maybe_addr), std::move(auth_token));
                    }
                    // we don't need to print a fail message, as it is logged prior to invoking with std::nullopt
                });
            }
        }
    }

    void SessionEndpoint::resolve_sns(std::string sns, std::function<void(std::optional<NetworkAddress>)> func)
    {
        Lock_t l{paths_mutex};
        if (not is_valid_sns(sns))
        {
            log::debug(logcat, "Invalid SNS name ({}) queried for lookup", sns);
            return func(std::nullopt);
        }

        log::debug(logcat, "Looking up SNS name {}", sns);

        auto remaining = std::make_shared<int>(0);
        auto response_handler = [sns, remaining, func = std::move(func)](auto resp) {
            int rem = --*remaining;
            if (rem < 0)
                return;  // Some other request beat us to it

            std::optional<NetworkAddress> client_addr;

            if (resp.ok())
            {
                try
                {
                    log::debug(logcat, "Call to ResolveSNS succeeded!");

                    auto enc = ResolveSNS::deserialize_response(oxenc::bt_dict_consumer{resp.body});

                    client_addr = enc.decrypt(sns);
                    if (client_addr)
                    {
                        log::debug(
                            logcat, "Successfully decrypted SNS record (name: {}, address: {})", sns, *client_addr);
                    }
                    else
                        log::warning(logcat, "Failed to decrypt SNS record (name: {})", sns);
                }
                catch (const std::exception& e)
                {
                    log::warning(logcat, "Exception during SNS response handling: {}", e.what());
                }
            }

            if (client_addr)
            {
                *remaining = 0;
                func(std::move(client_addr));
            }
            else if (rem == 0)
            {
                // If this is the last outstanding response, and still didn't succeed, then signal
                // the lookup failure to the callback:
                func(std::nullopt);
            }
        };

        auto name_hash = crypto::shorthash(as_bspan(sns));

        // TODO FIXME: this should not be fired down *every* path.
        for (auto& path : paths())
        {
            ++*remaining;
            log::debug(
                logcat, "Querying pivot:{} for name lookup (target: {})", path.terminal_rid().short_string(), sns);
            path.resolve_sns(name_hash, response_handler);
        }

        if (*remaining == 0)
        {
            log::warning(logcat, "Unable to resolve Lokinet SNS {}: we have no active paths", sns);
            func(std::nullopt);
        }
    }

    void SessionEndpoint::lookup_relay_contact(RouterID remote, std::function<void(std::optional<RelayContact>)> func)
    {
        if (auto* maybe_rc = router.node_db().get_rc(remote))
        {
            log::debug(logcat, "RelayContact for remote (rid: {}) found locally!", remote);
            return func(*maybe_rc);
        }

        log::debug(logcat, "Looking up RelayContact for remote (rid:{})", remote.to_network_address(true));

        auto remaining = std::make_shared<int>(0);

        auto response_handler = [this, remote, func = std::move(func), remaining](auto resp) {
            int rem = --*remaining;
            if (rem < 0)
            {  // Some other path handler already replied
                log::trace(logcat, "Dropping duplicate `fetch_rc` response (success: {})", resp.ok());
                return;
            }

            std::optional<RelayContact> rc;
            try
            {
                if (resp.ok())
                {
                    log::info(logcat, "Call to FetchRC succeeded!");
                    auto rcs = FetchRC::deserialize_response(router.netid(), oxenc::bt_dict_consumer{resp.body});

                    if (rcs.empty())
                        log::warning(logcat, "Received empty response from `fetch_rc` request!");
                    else if (rcs.size() > 1)
                        log::warning(
                            logcat, "Received more RC's than expected (n:{}) from `fetch_rc` request!", rcs.size());
                    else
                    {
                        log::debug(logcat, "Storing RelayContact for remote rid:{}", remote);
                        router.node_db().put_rc(rcs.front());
                        rc = std::move(rcs.front());
                    }
                }
                else
                {
                    std::optional<std::string> status = std::nullopt;
                    oxenc::bt_dict_consumer btdc{resp.body};

                    if (auto s = btdc.maybe<std::string>(messages::STATUS_KEY))
                        status = s;

                    log::warning(logcat, "Call to FetchRCs FAILED; reason: {}", status.value_or("<none given>"));
                }
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "An error occured processing fetched rc response: {}", e.what());
            }

            if (rc)
            {
                *remaining = 0;
                func(std::move(rc));
            }
            else if (rem == 0)
            {
                // We are the last path response and there have been no successes, so signal failure
                func(std::nullopt);
            }
        };

        Lock_t l{paths_mutex};

        for (const auto& [_, p] : _paths)
        {
            if (not p or not p->is_active())
                continue;

            ++*remaining;
            log::debug(
                logcat,
                "Querying pivot (rid:{}) for RelayContact lookup target (rid:{})",
                p->terminal_rid().short_string(),
                remote);

            p->fetch_relay_contact(remote, response_handler);
        }

        if (*remaining == 0)
        {
            log::warning(logcat, "RC lookup failed: no usable paths!");
            func(std::nullopt);
        }
    }

    void SessionEndpoint::lookup_client_intro(RouterID remote, std::function<void(std::optional<ClientContact>)> func)
    {
        if (auto maybe_intro = router.contact_db().get_decrypted_cc(remote))
        {
            log::debug(logcat, "Decrypted ClientContact for remote (rid: {}) found locally!", remote);
            return func(std::move(maybe_intro));
        }

        PubKey remote_key;
        if (!crypto::blind(remote_key, remote, crypto::blinding::CLIENT_CONTACT))
        {
            log::error(
                logcat,
                "Failed to blind remote address {}: this is most likely not a valid address",
                remote.to_network_address(false));
            func(std::nullopt);
            return;
        }

        log::debug(
            logcat,
            "Looking up ClientContact (key: {}) for remote (rid:{})",
            remote_key,
            remote.to_network_address(false));

        auto remaining = std::make_shared<int>(0);

        auto response_handler = [this, remote, func = std::move(func), remaining](auto resp) {
            int rem = --*remaining;
            if (rem < 0)
            {
                // Another path response already returned it
                log::trace(logcat, "Dropping duplicate `find_cc` response (success: {})", resp.ok());
                return;
            }

            std::optional<ClientContact> cc;
            try
            {
                if (resp.ok())
                {
                    log::info(logcat, "Call to FindClientContact succeeded!");
                    auto enc = FindClientContact::deserialize_response(oxenc::bt_dict_consumer{resp.body});

                    if (auto intro = enc.decrypt(remote))
                    {
                        log::debug(logcat, "Storing ClientContact for remote rid:{}", remote);
                        router.contact_db().put_cc(std::move(enc));
                        cc = std::move(intro);
                    }
                    else
                        log::warning(logcat, "Failed to decrypt returned EncryptedClientContact!");
                }
                else
                {
                    std::optional<std::string> status = std::nullopt;
                    oxenc::bt_dict_consumer btdc{resp.body};

                    if (auto s = btdc.maybe<std::string>(messages::STATUS_KEY))
                        status = s;

                    log::warning(
                        logcat, "Call to FindClientContact FAILED; reason: {}", status.value_or("<none given>"));
                }
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Exception: {}", e.what());
            }

            if (cc)
            {
                *remaining = 0;
                func(std::move(cc));
            }
            else if (rem == 0)
            {
                // Last chance and all failed, so trigger failure
                func(std::nullopt);
            }
        };

        Lock_t l{paths_mutex};

        for (const auto& [_, p] : _paths)
        {
            if (not p or not p->is_active())
                continue;

            ++*remaining;
            log::debug(
                logcat,
                "Querying pivot (rid:{}) for ClientContact lookup target (rid:{})",
                p->terminal_rid().short_string(),
                remote);

            p->find_client_contact(remote_key, response_handler);
        }

        if (*remaining == 0)
        {
            log::warning(logcat, "CC lookup failed: no usable paths!");
            func(std::nullopt);
        }
    }

    void SessionEndpoint::update_and_publish_localcc()
    {
        if (!router.config().network.is_reachable)
        {
            log::debug(logcat, "Not publishing CC: publishing is disabled by config");
            return;
        }

        log::debug(logcat, "Updating and publishing ClientContact...");

        auto now = llarp::time_now_ms();
        std::vector<ClientIntro> intros;
        for (const auto& [hopid, p] : _paths)
            if (p and p->is_active(now))
                intros.push_back(p->make_intro());

        client_contact.update_intros(std::move(intros));

        log::debug(logcat, "New ClientContact: {}", client_contact);
#ifndef NDEBUG
        log::trace(logcat, "ClientContact details:");
        log::trace(logcat, "Pubkey: {}", client_contact.pubkey());
        log::trace(logcat, "Intros ({}):", client_contact.intros().size());
        for (const auto& ci : client_contact.intros())
            log::trace(
                logcat,
                "    • {}, hopid: {}, expiry: {}",
                ci.relay.to_network_address(),
                ci.hop,
                std::chrono::floor<std::chrono::seconds>(ci.expires_in(now)));
#endif

        try
        {
            publish_client_contact(client_contact.encrypt_and_sign(cc_blind_keys));
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "ClientContact encryption/signing exception: {}", e.what());
        }
    }

    bool SessionEndpoint::validate(const NetworkAddress& remote, std::optional<std::string> maybe_auth)
    {
        auto& netconf = router.config().network;
        auto& tokens = netconf.auth_static_tokens;
        auto& whitelist = netconf.auth_whitelist;
        if (tokens.empty() && whitelist.empty())
            return true;  // No auth required

        if (maybe_auth && tokens.contains(*maybe_auth))
            return true;  // valid auth token

        if (whitelist.contains(remote))
            return true;  // valid address

        return false;
    }

    std::optional<std::variant<ipv4, ipv6>> SessionEndpoint::map_session(const session::Session& s)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (const auto& tun = router.tun_endpoint())
        {
            log::debug(logcat, "Successfully mapped inbound session; mapping session to local TUN IP");

            if (auto maybe_ipv4 = tun->map(s.remote()))
            {
                log::info(
                    logcat,
                    "TUN device successfully mapped session (remote: {}) to local ip: {}",
                    s.remote(),
                    *maybe_ipv4);
                return maybe_ipv4;
            }
            // TODO: ipv6

            // TODO: if this fails, we should close the session
            log::warning(logcat, "TUN device failed to map session (remote: {}) to local ip", s.remote());
            return std::nullopt;
        }

        // TODO: if we're not tun-based -- currently not allowing inbound sessions for non-tun

        return std::nullopt;
    }

    void SessionEndpoint::handle_session_init(std::vector<std::byte>&& payload, std::shared_ptr<path::Path> path)
    {
        std::shared_ptr<session::InboundSession> new_session{};
        try
        {
            new_session = std::make_shared<session::InboundClientSession>(*this, std::move(path), std::move(payload));
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Inbound session rejected: {}", e.what());
            return;
        }
        session_post_init(std::move(new_session));
    }

    void SessionEndpoint::handle_session_init(std::vector<std::byte>&& payload, std::shared_ptr<path::TransitHop> thop)
    {
        log::warning(logcat, "SessionEndpoint::handle_session_init (relay)");
        std::shared_ptr<session::InboundSession> new_session{};
        try
        {
            new_session = std::make_shared<session::InboundRelaySession>(*this, std::move(thop), std::move(payload));
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Inbound session rejected: {}", e.what());
            return;
        }
        log::warning(logcat, "SessionEndpoint::handle_session_init (relay) calling post_init");
        session_post_init(std::move(new_session));
    }

    void SessionEndpoint::session_post_init(std::shared_ptr<session::InboundSession> new_session)
    {
        // FIXME: for now only tun clients can have inbound sessions, but eventually that will
        //        not be the case and we'll need to "if tun" this.
        if (!map_session(*new_session))
        {
            log::warning(
                logcat,
                "Unable to map session to tun IP (or not allowing inbound sessions); dropping inbound session from {}",
                new_session->remote());
            return;
        }

        // TODO FIXME: this is racy, e.g. if two clients establish a session to each other at the
        // same time, then they can drop different ones.  We should instead use a decision metric
        // for dropping that decides the same way on both sides (e.g. prefer session initiated by
        // the side with the smaller pubkey).
        // FIXME: If the initiator does not get our response in time, they will try again
        // to establish a session; in that case we should replace what we have.
        auto& s = _sessions[new_session->remote()];
        auto* sptr = new_session.get();
        if (!s)
        {
            s = std::move(new_session);
            _session_tags[s->inbound_tag()] = s;
            // TODO: response with our inbound tag
        }
        log::warning(logcat, "sending session_init_accept");
        sptr->session_init_accept();
    }

    void SessionEndpoint::publish_client_contact(const EncryptedClientContact& ecc)
    {
        auto now = std::chrono::steady_clock::now();
        ++cc_count;
        // Send our CC down each inbound session so that everyone who is already connected to us
        // gets it pushed to them without having to always poll the network for updates.
        for (const auto& [addr, session] : _sessions)
        {
            // don't publish client contact to other end of outbound session
            if (session->is_outbound)
                return;
            log::debug(
                logcat,
                "Publishing ClientContact#{} to remote on inbound session (remote:{})",
                cc_count,
                session->remote());

            session->publish_client_contact(ecc);
        }

        // Pick four random inbound paths to publish on, and then on each one we send along a 0-3
        // location indicating where we want it to forward it (e.g. 0 means DHT-closest, 1 means 2nd
        // closest, etc.).
        std::vector<path::Path*> paths;
        paths.resize(path::CC_PUBLISH_LOCATIONS);
        auto end = std::ranges::sample(
            active_paths() | std::views::transform([](auto& p) { return &p; }),
            paths.begin(),
            path::CC_PUBLISH_LOCATIONS,
            llarp::csrng);
        paths.resize(std::distance(paths.begin(), end));
        if (paths.empty())
        {
            // This should be impossible: we should only have triggered a publish once we reached
            // our target number of active paths, but somehow found no active paths!
            log::error(logcat, "Internal error: attempt to publish CC with no active paths!");
            assert(false);
            return;
        }
        std::shuffle(paths.begin(), paths.end(), llarp::csrng);

        // Tracks number of successes and number of outstanding requests so that we can log success
        // (or error) when the last response comes back:
        auto remaining_success = std::make_shared<std::pair<int, int>>(path::CC_PUBLISH_LOCATIONS, 0);

        for (int location = 0; location < path::CC_PUBLISH_LOCATIONS; location++)
        {
            // % because we might have fewer than path::CC_PUBLISH_LOCATIONS, and if that happens we
            // just use some paths for multiple locations:
            auto& p = *paths[location % paths.size()];
            log::debug(logcat, "Publishing ClientContact to location {} via {}", location, p);
            p.publish_client_contact(
                ecc,
                location,
                [started = now, remaining_success, via = p.terminal_rid(), location, cc_num = cc_count](auto resp) {
                    auto elapsed =
                        std::chrono::round<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);

                    log::debug(
                        logcat,
                        "{} CC#{} publish[{}] via relay {} in {}",
                        resp.ok()            ? "Successful"
                            : resp.timed_out ? "Timeout during"
                                             : "Error during",
                        cc_num,
                        location,
                        via,
                        elapsed);
                    if (!resp.ok())
                        log::debug(logcat, "CC publish error response: {}", buffer_printer(resp.body));

                    auto& [remaining, success] = *remaining_success;
                    remaining--;
                    if (resp.ok())
                        success++;

                    if (not remaining)
                    {  // This is the last response
                        log::log(
                            logcat,
                            not success                                    ? log::Level::err
                                : success < path::CC_PUBLISH_LOCATIONS / 2 ? log::Level::warn
                                                                           : log::Level::info,
                            "CC#{} publish success to {}/{} publish locations in {}",
                            cc_num,
                            success,
                            path::CC_PUBLISH_LOCATIONS,
                            elapsed);
                    }
                });
        }
    }

    std::optional<std::string_view> SessionEndpoint::fetch_auth_token(const NetworkAddress& remote) const
    {
        std::optional<std::string_view> ret = std::nullopt;

        if (auto itr = _auth_tokens.find(remote); itr != _auth_tokens.end())
            ret = itr->second;

        return ret;
    }

    std::shared_ptr<session::Session> SessionEndpoint::remote_session(const NetworkAddress& remote)
    {
        assert(router.loop.inside());

        if (auto it = _sessions.find(remote); it != _sessions.end())
            return it->second;

        return nullptr;
    }

    std::shared_ptr<session::Session> SessionEndpoint::initiate_remote_session(
        const NetworkAddress& remote,
        std::function<void(session::Session& session)> on_attempted,
        std::optional<std::chrono::milliseconds> timeout)
    {
        return router.loop.call_get([this, &remote, &on_attempted, &timeout] {
            auto& s = _sessions[remote];
            if (s && !s->is_closed())
            {
                if (on_attempted)
                {
                    if (s->is_established())
                        on_attempted(*s);
                    else
                    {
                        assert(s->is_outbound);  // Inbound sessions are always established
                        // We have an already-in-progress but not-yet-established session, so just
                        // hook the callback up to it to be fired when it finishes establishing:
                        static_cast<session::OutboundSession*>(s.get())->on_established(
                            std::move(on_attempted), timeout);
                    }
                }
            }
            else
            {
                auto tag = next_tag();
                if (remote.client())
                    s = router.loop.make_shared<session::OutboundClientSession>(
                        remote, *this, tag, std::move(on_attempted), timeout);
                else
                    s = router.loop.make_shared<session::OutboundRelaySession>(
                        remote, *this, tag, std::move(on_attempted), timeout);
                _session_tags.emplace(tag, s);
            }

            return s;
        });
    }

    session_tag SessionEndpoint::next_tag()
    {
        // zero tag used to represent a session init for convenience
        while (_session_tags.contains(last_tag) || last_tag == 0)
            last_tag++;
        return last_tag;
    }
}  //  namespace llarp::handlers
