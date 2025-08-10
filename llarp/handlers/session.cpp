#include "session.hpp"

#include "llarp/contact/relay_contact.hpp"
#include "llarp/path/transit_hop.hpp"
#include "llarp/session/session.hpp"
#include "llarp/util/time.hpp"

#include <llarp/contact/contactdb.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/messages/fetch.hpp>
#include <llarp/messages/path.hpp>
#include <llarp/messages/session.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/bspan.hpp>

#include <oxenc/base32z.h>

#include <memory>

namespace llarp::handlers
{
    static auto logcat = log::Cat("SessionHandler");

    SessionEndpoint::SessionEndpoint(Router& r)
        : path::PathHandler{r, r.config().paths.inbound_paths, r.config().paths.inbound_hops()}
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

        client_contact = ClientContact{
            router.key_manager.derive_subkey(),
            router.key_manager.router_id(),
            netconf.srv_records,
            protocols,
            netconf.traffic_policy};

        should_publish_cc = netconf.is_reachable;
    }

    std::pair<size_t, size_t> SessionEndpoint::session_stats() const
    {
        return {
            _sessions.size(),
            std::ranges::count_if(std::views::values(_sessions), [](const auto& s) { return s->is_active(); }),
        };
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
                _session_tags.erase(s->tag());
            _sessions.erase(it);
        }
    }

    bool SessionEndpoint::recv_path_switch(
        session_tag t, HopID remote_pivot_txid, std::shared_ptr<session_path_interface> new_path)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (auto s = get_session<session::InboundClientSession>(t))
        {
            log::debug(
                logcat,
                "Successfully matched path-switch request to InboundSession over path:{}",
                new_path->to_string());

            s->recv_path_switch(std::move(remote_pivot_txid), std::move(new_path));
            return true;
        }

        return false;
    }

    bool SessionEndpoint::recv_path_switch(session_tag t, HopID remote_pivot_txid, HopID local_pivot_txid)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (auto s = get_session<session::InboundClientSession>(t))
        {
            // only OutboundSessions send path switch messages
            assert(s && !s->is_outbound());

            // PathHandler objects key their paths to the upstream rxid, so we use the conditional get_path
            if (auto* path = get_path_by_terminus(local_pivot_txid))
            {
                log::debug(
                    logcat,
                    "Successfully matched path-switch request to InboundSession over path:{}",
                    path->to_string());
                s->set_remote_pivot_tx(remote_pivot_txid);
                s->set_new_current_path(path->shared_from_this());
                return true;
            }

            log::warning(logcat, "Received path-switch request for unknown local pivot txid: {}", local_pivot_txid);
        }
        else
            log::warning(logcat, "Received path-switch request for unknown session (tag:{})", t);

        return false;
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

        if (_cc_publisher)
        {
            if (_cc_publisher->is_running())
                _cc_publisher->stop();

            _cc_publisher.reset();
            log::trace(logcat, "ClientContact publish ticker stopped!");
        }

        if (_path_rotater)
        {
            if (_path_rotater->is_running())
                _path_rotater->stop();

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

    void SessionEndpoint::update_paths()
    {
        int have = num_paths();
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

        // Exclude any inbound pivots we are already using so that we diversify:
        auto filter = [this](const RemoteRC& rc) {
            const auto& rid = rc.router_id();
            for (const auto& p : paths())
                if (p.terminal_rid() == rid)
                    return false;

            return not router.router_profiling().is_bad_for_path(rid, 1);
        };

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
                build(std::span{&*only_hop, 1});
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
            for (const llarp::RemoteRC& rc : new_pivots)
            {
                log::debug(logcat, "Selected new inbound path terminus {}", rc.router_id().short_string());
                auto hops = aligned_hops_to_remote(rc.router_id());
                if (!hops)
                    continue;  // No need to warn: the call above should already if it fails

                build(*hops);
            }
        }

        // FIXME TODO: do I know how many path builds are currently in progress, so that I don't end
        // up building too many paths (e.g. if one tick builds then the next tick fires before the
        // first builds finish?)
    }

    void SessionEndpoint::start_tickers()
    {
        if (!router.is_service_node and should_publish_cc)
        {
            log::trace(logcat, "Starting ClientContact publish ticker...");

            router.loop.call_later(uniform_duration_distribution{5s, 10s}(llarp::csrng), [this] {
                update_and_publish_localcc();
                _cc_publisher = router.loop.call_every(CC_PUBLISH_INTERVAL, [this] {
                    log::critical(logcat, "TESTNET: Skipping ClientContact publish!");
                    // TODO FIXME
                    // update_and_publish_localcc();
                });
            });
        }
        else
            log::info(logcat, "SessionEndpoint configured to NOT publish ClientContact...");
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
        auto response_handler = [sns, remaining, func = std::move(func)](quic::message m) {
            int rem = --*remaining;
            if (rem < 0)
                return;  // Some other request beat us to it

            std::optional<NetworkAddress> client_addr;

            if (m)
            {
                try
                {
                    log::debug(logcat, "Call to ResolveSNS succeeded!");

                    auto enc = ResolveSNS::deserialize_response(oxenc::bt_dict_consumer{m.body()});

                    client_addr = enc.decrypt(sns);
                    if (client_addr)
                    {
                        log::debug(
                            logcat,
                            "Successfully decrypted SNS record (name: {}, address: {})",
                            sns,
                            client_addr->to_string());
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

    void SessionEndpoint::lookup_relay_contact(RouterID remote, std::function<void(std::optional<RemoteRC>)> func)
    {
        if (auto* maybe_rc = router.node_db().get_rc(remote))
        {
            log::debug(logcat, "RelayContact for remote (rid: {}) found locally!", remote);
            return func(*maybe_rc);
        }

        log::debug(logcat, "Looking up RelayContact for remote (rid:{})", remote.to_network_address(true));

        auto remaining = std::make_shared<int>(0);

        auto response_handler = [this, remote, func = std::move(func), remaining](quic::message m) {
            int rem = --*remaining;
            if (rem < 0)
            {  // Some other path handler already replied
                log::trace(logcat, "Dropping duplicate `fetch_rc` response (success: {})", not m.is_error());
                return;
            }

            std::optional<RemoteRC> rc;
            try
            {
                if (m)
                {
                    log::info(logcat, "Call to FetchRC succeeded!");
                    auto rcs = FetchRC::deserialize_response(router.netid(), oxenc::bt_dict_consumer{m.body()});

                    if (rcs.empty())
                    {
                        log::warning(logcat, "Received empty response from `fetch_rc` request!");
                        return;
                    }

                    if (rcs.size() > 1)
                    {
                        log::warning(
                            logcat, "Received more RC's than expected (n:{}) from `fetch_rc` request!", rcs.size());
                        return;
                    }

                    log::debug(logcat, "Storing RelayContact for remote rid:{}", remote);
                    router.node_db().put_rc(rcs.front());
                    rc = std::move(rcs.front());
                }
                else
                {
                    std::optional<std::string> status = std::nullopt;
                    oxenc::bt_dict_consumer btdc{m.body()};

                    if (auto s = btdc.maybe<std::string>(messages::STATUS_KEY))
                        status = s;

                    log::warning(logcat, "Call to FetchRCs FAILED; reason: {}", status.value_or("<none given>"));
                }
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Exception: {}", e.what());
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

        auto remote_key = hash_key::derive_from_rid(remote);

        log::debug(
            logcat,
            "Looking up ClientContact (key: {}) for remote (rid:{})",
            remote_key,
            remote.to_network_address(false));

        auto remaining = std::make_shared<int>(0);

        auto response_handler = [this, remote, func = std::move(func), remaining](quic::message m) {
            int rem = --*remaining;
            if (rem < 0)
            {
                // Another path response already returned it
                log::trace(logcat, "Dropping duplicate `find_cc` response (success: {})", not m.is_error());
                return;
            }

            std::optional<ClientContact> cc;
            try
            {
                if (m)
                {
                    log::info(logcat, "Call to FindClientContact succeeded!");
                    auto enc = FindClientContact::deserialize_response(oxenc::bt_dict_consumer{m.body()});

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
                    oxenc::bt_dict_consumer btdc{m.body()};

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

    bool SessionEndpoint::update_and_publish_localcc(bool force)
    {
        // TODO FIXME: implement force

        if (!should_publish_cc)
        {
            log::debug(logcat, "Nothing to publish: ClientContact publishing is disabled");
            return true;
        }

        log::debug(logcat, "Updating and publishing ClientContact...");

        auto now = llarp::time_now_ms();
        std::vector<ClientIntro> intros;
        for (const auto& [hopid, p] : _paths)
            if (p and p->is_active(now))
                intros.push_back(p->intro);
        if (intros.empty())
        {
            log::warning(logcat, "Unable to publish ClientContact: we have no usable paths/intros");
            return false;
        }

        client_contact.update_intros(std::move(intros));

        try
        {
            auto enc = client_contact.encrypt_and_sign();

#ifndef NDEBUG
            assert(enc.verify());
            {
                auto decrypt = enc.decrypt(router.local_rid());
                assert(decrypt);
                assert(*decrypt == client_contact);
            }
#endif

            publish_client_contact(enc);
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "ClientContact encryption/signing exception: {}", e.what());
        }
        return false;
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

    std::optional<session_tag> SessionEndpoint::create_inbound_session(
        NetworkAddress initiator,
        HopID remote_pivot_txid,
        std::shared_ptr<session_path_interface> path,
        shared_kx_data kx_data)
    {
        // TODO FIXME: this is making a random tag, but that isn't right as it could conflict.
        // Rather we should be retrying until we find one that isn't in _session_tags so that it
        // can't possible conflict below.
        session_tag tag{protocols};

        std::shared_ptr<session::Session> session;
        if (router.is_service_node)
            session = std::make_shared<session::InboundRelaySession>(
                initiator, std::move(path), *this, remote_pivot_txid, tag, std::move(kx_data));
        else
            session = std::make_shared<session::InboundClientSession>(
                initiator, std::move(path), *this, remote_pivot_txid, tag, std::move(kx_data));

        if (!map_session(*session))
        {
            log::warning(
                logcat,
                "Unable to map session to tun IP (or not allowing inbound sessions); dropping inbound session from {}",
                initiator);
            return std::nullopt;
        }

        // TODO FIXME: this is racy, e.g. if two clients establish a session to each other at the
        // same time, then they can drop different ones.  We should instead use a decision metric
        // for dropping that decides the same way on both sides (e.g. prefer session initiated by
        // the side with the smaller pubkey).
        auto& s = _sessions[initiator];
        if (!s)
        {
            s = std::move(session);
            auto& st = _session_tags[s->tag()];
            if (!st)
                st = s;
            else
            {
                // TODO FIXME: we should be producing the remote the session tag to use with us,
                // rather than using the remote's tag on both sides, so that we can't conflict like
                // this.
                log::error(logcat, "Dropping inbound session because of conflicting session tag");
                _sessions.erase(initiator);
                return std::nullopt;
            }
        }
        else
        {
            log::warning(logcat, "Dropping duplicate inbound session with initiator {}", initiator);
            return std::nullopt;
        }

        return s->tag();
    }

    static void publish_cc_cb(quic::message m)
    {
        if (m)
        {
            log::debug(logcat, "Call to PublishClientContact succeeded!");
        }
        else
        {
            std::optional<std::string> status = std::nullopt;
            try
            {
                oxenc::bt_dict_consumer btdc{m.body()};

                if (auto s = btdc.maybe<std::string>(messages::STATUS_KEY))
                    status = s;
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Exception: {}", e.what());
            }

            log::critical(logcat, "Call to PublishClientContact FAILED; reason: {}", status.value_or("<none given>"));
        }
    }

    void SessionEndpoint::publish_client_contact(const EncryptedClientContact& ecc)
    {
        log::trace(logcat, "Publishing new EncryptedClientContact: {}", ecc.bt_payload());

        // Send our CC down each inbound session so that everyone who is already connected to us
        // gets it pushed to them without having to always poll the network for updates.
        for (const auto& [addr, session] : _sessions)
        {
            // don't publish client contact to other end of outbound session
            if (session->is_outbound())
                return;
            log::debug(logcat, "Publishing ClientContact to remote on inbound session (remote:{})", session->remote());

            session->publish_client_contact(ecc, publish_cc_cb);
        }

        // Publish it down our established inbound paths; each terminus should then forward it on
        // to the 4 best locations.
        //
        // TODO FIXME This doesn't right: we could have loads of paths, *and* every one we send
        // would amplify by 4 at each terminus, which is way too much network data.  Instead we
        // should perhaps do something like:
        // - "publish to 1st best" -> send down 1 path
        // - "publish to 2nd best" -> send down 1 path
        // - "publish to 3rd best" -> send down 1 path
        // - "publish to 4th best" -> send down 1 path
        //
        // where we try (if possible) to use 4 different paths for the 4 requests, so that there
        // isn't any amplification at all.  If some fail that doesn't matter because there is
        // already 4x redundancy built in here.
        {
            Lock_t l{paths_mutex};

            for (auto& p : active_paths())
            {
                log::debug(logcat, "Publishing ClientContact on {}", p);
                p.publish_client_contact(ecc, publish_cc_cb);
            }
        }
    }

    std::optional<std::string_view> SessionEndpoint::fetch_auth_token(const NetworkAddress& remote) const
    {
        std::optional<std::string_view> ret = std::nullopt;

        if (auto itr = _auth_tokens.find(remote); itr != _auth_tokens.end())
            ret = itr->second;

        return ret;
    }

    /** Client Session Initiation Message Structure:
        - 'k' : next HopID
        - 'n' : symmetric nonce
        - 'x' : encrypted payload
            PATH MESSAGE ONION LAYER ('outer payload')
            - 'e' : request endpoint ('path_control')
            - 'p' : request payload
                - 'k' : next HopID
                - 'n' : symmetric nonce
                - 'x' : encrypted payload
                    PIVOT RELAY LAYER ('intermediate payload')
                    - 'e' : request endpoint ('path_control')
                    - 'p' : request payload
                        - 'k' : remote client intro pivot txid, (NOT rx)
                        - 'n' : symmetric nonce
                        - 'x' : encrypted payload
                            REMOTE CLIENT LAYER ('inner payload')
                            - 'e' : request endpoint ('session_init')
                            - 'p' : request payload
                                - 'k' : shared pubkey used to derive symmetric key
                                - 'n' : symmetric nonce
                                - 'x' : encrypted payload
                                    - 'i' : RouterID of initiator
                                    - 'p' : HopID at the pivot taken from remote ClientIntro
                                    - 's' : session_tag for current session
                                    - 't' : Use Tun interface (bool)
                                    - 'u' : Authentication field
                                        - bt-encoded dict, values TBD
    */
    // TESTNET: TODO: DRY out the following two functions
    void SessionEndpoint::_make_client_session(
        std::vector<ClientIntro> intros,
        NetworkAddress remote,
        ClientIntro remote_intro,
        std::shared_ptr<path::Path> path,
        std::function<void(bool)> cb)
    {
        auto pivot_txid = remote_intro.pivot_txid;
        // intros.emplace(std::move(remote_intro));

        // internal payload for remote client
        auto [inner_payload, shared_secret] = InitiateSession::serialize_encrypt(
            router.local_rid(), remote.router_id(), path->terminal_hopid(), pivot_txid, fetch_auth_token(remote));
        log::trace(logcat, "inner payload: {}", buffer_printer{inner_payload});

        auto intermediate_payload = PATH::CONTROL::serialize_aligned(std::move(inner_payload), pivot_txid);

        path->send_path_control_message(
            "path_control",
            intermediate_payload,
            [this,
             remote,
             path,
             remote_pivot_txid = pivot_txid,
             remote_intros = std::move(intros),
             hook = std::move(cb),
             session_keys = std::move(kx_data)](quic::message m) mutable {
                auto pending_packets = std::move(pending_sessions[remote]);
                auto pending_hooks = std::move(pending_session_hooks[remote]);
                pending_sessions.erase(remote);
                pending_session_hooks.erase(remote);
                if (m)
                {
                    log::debug(logcat, "Call to initiate OutboundClientSession succeeded!");
                    session_tag tag;

                    try
                    {
                        tag = InitiateSession::deserialize_response(oxenc::bt_dict_consumer{m.body()});
                    }
                    catch (const std::exception& e)
                    {
                        // TESTNET: TODO: close session here?
                        log::warning(logcat, "Exception: {}", e.what());
                        return;
                    }

                    log::debug(logcat, "Remote client has provided session tag: {}", tag);

                    auto session = std::make_shared<session::OutboundClientSession>(
                        remote,
                        *this,
                        std::move(path),
                        std::move(remote_pivot_txid),
                        std::move(tag),
                        std::move(remote_intros),
                        std::move(session_keys));

                    auto [s, _] = _sessions.insert_or_assign(std::move(remote), session);
                    assert(s->is_active());

                    log::trace(logcat, "Outbound session to {} successfully created...", session->remote());

                    if (pending_packets.size())
                    {
                        log::debug(
                            logcat,
                            "Session to {} established, sending {} pending packets.",
                            session->remote(),
                            pending_packets.size());
                        for (auto& pkt : pending_packets)
                            session->send_path_data_message(pkt.span(), pkt.protocol());
                    }
                    if (hook)
                        hook(true);
                    for (auto& h : pending_hooks)
                        h(true);
                    return;
                }
                else
                {
                    std::optional<std::string> status = std::nullopt;
                    try
                    {
                        oxenc::bt_dict_consumer btdc{m.body()};

                        if (auto s = btdc.maybe<std::string>(messages::STATUS_KEY))
                            status = s;
                    }
                    catch (const std::exception& e)
                    {
                        log::warning(logcat, "Exception: {}", e.what());
                    }

                    log::critical(
                        logcat,
                        "Call to initiate OutboundClientSession FAILED; reason: {}",
                        status.value_or("<none given>"));
                    if (hook)
                        hook(false);
                    for (auto& h : pending_hooks)
                        h(true);
                }
            });

        log::debug(logcat, "message sent...");
    }

    void SessionEndpoint::_make_relay_session(
        RemoteRC rc, NetworkAddress remote, std::shared_ptr<path::Path> path, std::function<void(bool)> cb)
    {
        const auto& pivot_txid = path->pivot().txid;
        std::string payload = InitiateSession::serialize(
            _router.local_rid(), pivot_txid, pivot_txid, fetch_auth_token(remote), !_router.embedded());

        log::trace(logcat, "payload: {}", buffer_printer{payload});

        path->send_path_control_message(
            "session_init",
            as_bspan(payload),
            [this,
             rc = std::move(rc),
             remote,
             path,
             pivot_txid,
             hook = std::move(cb),
             session_keys = path->hops.back().kx](quic::message m) mutable {
                auto pending_packets = std::move(pending_sessions[remote]);
                auto pending_hooks = std::move(pending_session_hooks[remote]);
                pending_sessions.erase(remote);
                pending_session_hooks.erase(remote);

                if (m)
                {
                    log::debug(logcat, "Call to initiate OutboundRelaySession succeeded!");
                    session_tag tag;

                    try
                    {
                        tag = InitiateSession::deserialize_response(oxenc::bt_dict_consumer{m.body()});
                    }
                    catch (const std::exception& e)
                    {
                        // TESTNET: TODO: close session here?
                        log::warning(logcat, "Exception: {}", e.what());
                        return;
                    }

                    log::debug(logcat, "Remote relay has provided session tag: {}", tag);

                    auto session = std::make_shared<session::OutboundRelaySession>(
                        remote, *this, std::move(path), std::move(tag), std::move(pivot_txid), std::move(session_keys));

                    auto [s, _] = _sessions.insert_or_assign(std::move(remote), session);
                    assert(s->is_active());

                    log::trace(logcat, "Outbound session to {} successfully created...", session->remote());

                    if (pending_packets.size())
                    {
                        log::debug(
                            logcat,
                            "Session to {} established, sending {} pending packets.",
                            session->remote(),
                            pending_packets.size());
                        for (auto& pkt : pending_packets)
                            session->send_path_data_message(pkt.span(), pkt.protocol());
                    }
                    if (hook)
                        hook(true);
                    for (auto& h : pending_hooks)
                        h(true);
                }
                else
                {
                    std::optional<std::string> status = std::nullopt;
                    try
                    {
                        oxenc::bt_dict_consumer btdc{m.body()};

                        if (auto s = btdc.maybe<std::string>(messages::STATUS_KEY))
                            status = s;
                    }
                    catch (const std::exception& e)
                    {
                        log::warning(logcat, "Exception: {}", e.what());
                    }

                    log::critical(
                        logcat,
                        "Call to initiate OutboundRelaySession FAILED; reason: {}",
                        status.value_or("<none given>"));
                    if (hook)
                        hook(false);
                    for (auto& h : pending_hooks)
                        h(false);
                }
            });

        log::debug(logcat, "message sent...");
    }

    void SessionEndpoint::_make_relay_session_path(RemoteRC rc, NetworkAddress remote, std::function<void(bool)> cb)
    {
        log::debug(logcat, "{} called", __PRETTY_FUNCTION__);

        path_build_recursive(
            SESSION_PATH_BUILD_ATTEMPTS,
            rc,
            remote,
            [this, rc, remote, cb](std::shared_ptr<path::Path> new_path) {
                log::info(logcat, "Path build to remote:{} succeeded, initiating session!", remote);
                _make_relay_session(std::move(rc), std::move(remote), std::move(new_path), std::move(cb));
            },
            false);
    }

    void SessionEndpoint::_make_client_session_path(
        sorted_intro_set intros, NetworkAddress remote, std::function<void(bool)> cb)
    {
        log::debug(logcat, "{} called", __PRETTY_FUNCTION__);

        path_build_recursive(
            intros,
            remote,
            [this, intros, remote, cb](std::shared_ptr<path::Path> new_path, ClientIntro remote_intro) mutable {
                log::info(logcat, "Path build to remote:{} succeeded, initiating session!", remote);
                return _make_client_session(
                    std::move(intros), std::move(remote), std::move(remote_intro), std::move(new_path), std::move(cb));
            },
            false);
    }
    std::shared_ptr<session::Session> SessionEndpoint::remote_session(const NetworkAddress& remote)
    {
        assert(router.loop.inside());

        if (auto it = _session_by_addr.find(remote); it != _session_by_addr.end())
            return it->second;
    }

    void SessionEndpoint::initiate_remote_session(
        const NetworkAddress& remote, std::function<void(bool timeout)> on_established)
    {
        std::function<void(const OutboundSession& s)> on_est;
        if (on_established)
            on_est = [cb = std::move(on_established)](const OutboundSession& s) { cb(s.is_established()); };
        router.loop.call_get([this, &remote, &on_est] {
            auto& s = _sessions[remote];
            if (s && !s->is_closed())
            {
                if (on_est)
                    if (s->is_established())
                        on_est(true);
                    else
                    {
                        assert(s->is_outbound());  // Inbound sessions are always established
                        // We have an already-in-progress but not-yet-established session, so just
                        // hook the callback up to it to be fired when it finishes establishing:
                        static_cast<OutboundSession*>(s.get())->on_established(std::move(on_est));
                    }
            }

            std::shared_ptr<session::Session> sess;
            if (remote.is_client())
                sesh = router.loop.make_shared<OutboundClientSession>(remote, *this, std::move(on_est));
            else
                sesh = router.loop.make_shared<OutboundRelaySession>(remote, *this, std::move(on_est));
            _sessions[remote] = std::move(sesh);
        });
    }

    void SessionEndpoint::_initiate_client_session(NetworkAddress remote, std::function<void(bool)> cb)
    {
        _router.loop()->call([this, remote, cb = std::move(cb)]() mutable {
            lookup_client_intro(
                remote.router_id(), [this, remote, cb = std::move(cb)](std::optional<ClientContact> cc) mutable {
                    if (cc)
                    {
                        log::debug(logcat, "Session initiation returned client contact: {}", cc->to_string());
                        _make_client_session_path(std::move(*cc).intros(), remote, std::move(cb));
                    }
                    else
                    {
                        log::warning(logcat, "Failed to initiate session at 'find_cc' (target:{})", remote);
                        cb(false);
                    }
                });
        });
    }

    void SessionEndpoint::_initiate_relay_session(NetworkAddress remote, std::function<void(bool)> cb)
    {
        _router.loop()->call([this, remote, cb = std::move(cb)]() mutable {
            lookup_relay_contact(
                remote.router_id(), [this, remote, cb = std::move(cb)](std::optional<RemoteRC> rc) mutable {
                    if (rc)
                    {
                        log::debug(logcat, "Session initiation returned RC: {}", rc->to_string());
                        _make_relay_session_path(std::move(*rc), remote, std::move(cb));
                    }
                    else
                    {
                        log::warning(logcat, "Failed to initiate session at `fetch_rcs` (target:{})", remote);
                        cb(false);
                    }
                });
        });
    }

    bool SessionEndpoint::have_pending_session(const NetworkAddress& remote)
    {
        return pending_sessions.contains(remote);
    }

    void SessionEndpoint::queue_session_packet(const NetworkAddress& remote, IPPacket pkt)
    {
        if (pending_sessions.contains(remote))
        {
            // FIXME: Perhaps we should keep the *last* 100 (dropping older ones) instead of the first
            // 100 (dropping new ones)?
            if (pending_sessions[remote].size() < 100)  // FIXME: constant
                pending_sessions[remote].push_back(std::move(pkt));
        }
    }

}  //  namespace llarp::handlers
