#include "session.hpp"

#include <llarp/contact/contactdb.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/messages/fetch.hpp>
#include <llarp/messages/path.hpp>
#include <llarp/messages/session.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/bspan.hpp>

#include <oxenc/base32z.h>

namespace llarp::handlers
{
    static auto logcat = log::Cat("SessionHandler");

    SessionEndpoint::SessionEndpoint(Router& r) : path::PathHandler{r, path::DEFAULT_PATHS_HELD, path::DEFAULT_LEN}
    {
        const auto& netconf = _router.config().network;

        _auth_tokens = netconf.exit_auths;

        // *All* clients currently support speaking via QUIC tunnel:
        protocols = protocol_flag::QUIC_TUNNEL;
        if (!_router.embedded())
        {
            // raw IPv4/IPv6/exit traffic all require a full tun interface.

            protocols = protocol_flag::IPV4;
            if (netconf.enable_ipv6)
                protocols |= protocol_flag::IPV6;
            if (_router.is_exit_node())
                protocols |= protocol_flag::EXIT;
        }

        client_contact = ClientContact{
            _router.key_manager.derive_subkey(),
            _router.key_manager.router_id(),
            netconf.srv_records,
            protocols,
            netconf.traffic_policy};

        should_publish_cc = netconf.is_reachable;
    }

    const std::shared_ptr<quic::Loop>& SessionEndpoint::loop() { return _router.loop(); }

    std::pair<size_t, bool> SessionEndpoint::session_stats() const
    {
        return {_sessions.count(), _router.is_exit_node()};
    }

    void SessionEndpoint::unmap_session(NetworkAddress remote)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (auto& tun = _router.tun_endpoint())
            tun->unmap_session_to_local_ip(remote);

        _sessions.unmap(remote);
        log::info(logcat, "Session (remote:{}) closed and unmapped!", remote);
    }

    void SessionEndpoint::close_session(std::shared_ptr<session::BaseSession>& s, bool send_close)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        s->stop_session(send_close);
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
            if (auto path = find_path(
                    [&local_pivot_txid](const path::Path& p) { return p.pivot().txid() == local_pivot_txid; }))
            {
                log::debug(
                    logcat,
                    "Successfully matched path-switch request to InboundSession over path:{}",
                    path->to_string());
                s->set_remote_pivot_tx(remote_pivot_txid);
                s->set_new_current_path_interface(std::move(path));
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

        if (auto s = _sessions.get_session(remote))
        {
            close_session(s, send_close);
            return true;
        }

        log::warning(logcat, "Could not find session (remote:{}) to close!", remote);
        return false;
    }

    bool SessionEndpoint::close_session(session_tag t, bool send_close)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (auto s = _sessions.get_session(t))
        {
            close_session(s, send_close);
            return true;
        }

        log::warning(logcat, "Could not find session (tag:{}) to close!", t);
        return false;
    }

    void SessionEndpoint::tick(std::chrono::milliseconds now)
    {
        log::trace(logcat, "SessionEndpoint ticking outbound sessions...");
        _sessions.tick_outbounds(now);

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

        if (send_close)
        {
            std::promise<void> prom;

            _router.loop()->call([&]() mutable {
                _sessions.for_each([](session::BaseSession& s) { s.send_path_close(); });
                prom.set_value();
            });

            prom.get_future().get();
            log::debug(logcat, "Dispatched all path close messages!");
        }

        _sessions.clear_sessions();

        path::PathHandler::stop();
    }

    void SessionEndpoint::drop_oldest_path()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        Lock_t l{paths_mutex};

        auto oldest = get_oldest_path();
        log::debug(logcat, "Dropping oldest path: {}", oldest->to_string());
        drop_path(oldest);
    }

    void SessionEndpoint::rotate_paths()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        Lock_t l{paths_mutex};

        auto maybe_hops = get_hops_to_random();

        if (not maybe_hops)
        {
            log::warning(logcat, "Failed to get hops for path-build to random");
            return;
        }

        path::PathHandler::rotate_paths(std::move(*maybe_hops));
    }

    void SessionEndpoint::path_rotation_succeeded(const std::shared_ptr<path::Path>& new_path)
    {
        log::info(logcat, "SessionEndpoint successfully rotated in new path: {}", new_path->to_string());
        // log::info(logcat, "SessionEndpoint successfully rotated in new path: {}", new_path->debug_string());
        path_build_succeeded(new_path);
        drop_oldest_path();
        update_and_publish_localcc();
    }

    std::optional<std::vector<RemoteRC>> SessionEndpoint::get_hops_to_random()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        auto filter = [this](const RemoteRC& rc) mutable {
            const auto& rid = rc.router_id();

            for (const auto& [_, p] : _paths)
                if (p and p->pivot().router_id() == rid)
                    return false;

            return not _router.router_profiling().is_bad_for_path(rid, 1);
        };

        if (auto maybe = _router.node_db().get_random_rc(filter))
            return aligned_hops_to_remote(maybe->router_id());

        return std::nullopt;
    }

    void SessionEndpoint::build_more(size_t n)
    {
        log::debug(logcat, "SessionEndpoint building {} paths to random remotes (needed: {})", n, num_paths_desired);

        for (size_t count = 0; count < n; count++)
        {
            if (!build_path_to_random())
            {
                log::warning(logcat, "SessionEndpoint only initiated {} path-builds (needed: {})", count, n);
                return;
            }
        }
        log::debug(logcat, "SessionEndpoint successfully initiated {} path-builds", n);
    }

    void SessionEndpoint::start_tickers()
    {
        if (!_router.is_service_node() and should_publish_cc)
        {
            log::trace(logcat, "Starting ClientContact publish ticker...");

            _router.loop()->call_later(uniform_duration_distribution{5s, 10s}(llarp::csrng), [this] {
                update_and_publish_localcc();
                _cc_publisher = _router.loop()->call_every(CC_PUBLISH_INTERVAL, [this] {
                    log::critical(logcat, "TESTNET: Skipping ClientContact publish!");
                    // TODO FIXME
                    // update_and_publish_localcc();
                });
            });

            log::trace(logcat, "Starting path rotation ticker...");
            _path_rotater = _router.loop()->call_every(path::PATH_ROTATION_INTERVAL, [this] { rotate_paths(); });
        }
        else
            log::info(logcat, "SessionEndpoint configured to NOT publish ClientContact...");
    }

    void SessionEndpoint::resolve_sns_mappings()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        auto& sns_ranges = _router.config().exit.sns_ranges;

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

        auto& sns_auths = _router.config().network.sns_exit_auths;

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
        for (const auto& [_, path] : _paths)
        {
            if (not path or not path->is_active())
                continue;

            ++*remaining;
            log::debug(
                logcat,
                "Querying pivot:{} for name lookup (target: {})",
                path->pivot().router_id().short_string(),
                sns);
            path->resolve_sns(name_hash, response_handler);
        }

        if (*remaining == 0)
        {
            log::warning(logcat, "Unable to resolve Lokinet SNS {}: we have no active paths", sns);
            func(std::nullopt);
        }
    }

    void SessionEndpoint::lookup_relay_contact(RouterID remote, std::function<void(std::optional<RemoteRC>)> func)
    {
        if (auto* maybe_rc = _router.node_db().get_rc(remote))
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
                    auto rcs = FetchRC::deserialize_response(_router.netid(), oxenc::bt_dict_consumer{m.body()});

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
                    _router.node_db().put_rc(rcs.front());
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
                p->pivot().router_id().short_string(),
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
        if (auto maybe_intro = _router.contact_db().get_decrypted_cc(remote))
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
                        _router.contact_db().put_cc(std::move(enc));
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
                p->pivot().router_id().short_string(),
                remote);

            p->find_client_contact(remote_key, response_handler);
        }

        if (*remaining == 0)
        {
            log::warning(logcat, "CC lookup failed: no usable paths!");
            func(std::nullopt);
        }
    }

    void SessionEndpoint::_localcc_update_fail()
    {
        _router.loop()->call([this]() mutable {
            log::warning(
                logcat,
                "Failed to query enough client intros from current paths! Building more paths to publish contact!");
            return build_more(1);
        });
    }

    void SessionEndpoint::update_and_publish_localcc()
    {
        if (should_publish_cc)
        {
            log::debug(logcat, "Updating and publishing ClientContact...");
            auto intros = get_local_client_intros();
            if (intros.empty())
                return _localcc_update_fail();
            client_contact.update_intros(std::move(intros));
            _update_and_publish_localcc();
        }
        else
            log::warning(logcat, "Local instance not configured to publish ClientContact!");
    }

    void SessionEndpoint::_update_and_publish_localcc()
    {
        try
        {
            auto enc = client_contact.encrypt_and_sign();

            if (not enc.verify())
                log::critical(logcat, "COULD NOT VERIFY ENCRYPTEDCLIENTCONTACT");

            if (auto decrypt = enc.decrypt(_router.local_rid()))
            {
                if (client_contact == *decrypt)
                    log::trace(logcat, "Decrypted ClientContact is EQUAL to the original!");
                else
                    log::critical(logcat, "Decrypted ClientContact is NOT EQUAL to the original!");
            }
            else
                log::critical(logcat, "COULD NOT DECRYPT ENCRYPTEDCLIENTCONTACT");

            if (publish_client_contact(enc))
                log::info(logcat, "Successfully republished updated EncryptedClientContact!");
            else
                log::warning(logcat, "Failed to republish updated EncryptedClientContact!");
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "ClientContact encryption/signing exception: {}", e.what());
        }
    }

    bool SessionEndpoint::validate(const NetworkAddress& remote, std::optional<std::string> maybe_auth)
    {
        auto& netconf = _router.config().network;
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

    static constexpr auto success_msg = "SessionEndpoint successfully created and mapped InboundSession object!"sv;

    std::optional<std::variant<ipv4, ipv6>> SessionEndpoint::map_session(const session::BaseSession& s)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (const auto& tun = _router.tun_endpoint())
        {
            log::trace(logcat, "{} Instructing lokinet TUN device to create mapped route...", success_msg);

            if (auto maybe_ipv4 = tun->map_session_to_local_ip(s.remote()))
            {
                log::info(
                    logcat,
                    "TUN device successfully routing session (remote: {}) via local ip: {}",
                    s.remote(),
                    *maybe_ipv4);
                return maybe_ipv4;
            }
            // TODO: ipv6

            // TODO: if this fails, we should close the session
            log::warning(logcat, "TUN device failed to route session (remote: {}) to local ip", s.remote());
            return std::nullopt;
        }

        // TODO: if we're not tun-based -- currently not allowing inbound sessions for non-tun

        return std::nullopt;
    }

    std::optional<session_tag> SessionEndpoint::prefigure_session(
        NetworkAddress initiator,
        HopID remote_pivot_txid,
        std::shared_ptr<session_path_interface> path,
        shared_kx_data kx_data,
        bool use_tun)
    {
        session_tag tag{protocols};

        std::shared_ptr<session::BaseSession> s;

        if (_router.is_service_node())
        {
            auto session = std::make_shared<session::InboundRelaySession>(
                initiator, std::move(path), *this, std::move(remote_pivot_txid), tag, use_tun, std::move(kx_data));

            s = _sessions.insert_or_assign(std::move(initiator), std::move(session)).first;
        }
        else
        {
            auto session = std::make_shared<session::InboundClientSession>(
                initiator, std::move(path), *this, std::move(remote_pivot_txid), tag, use_tun, std::move(kx_data));

            s = _sessions.insert_or_assign(std::move(initiator), std::move(session)).first;
        }

        assert(s and s->is_active());

        // TODO: remove ifdef (and change this) once we allow inbound sessions for liblokinet clients
        if (auto maybe_ip = map_session(*s))
            return tag;

        return std::nullopt;
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

    bool SessionEndpoint::publish_client_contact(const EncryptedClientContact& ecc)
    {
        bool ret{true};

        log::trace(logcat, "Publishing new EncryptedClientContact: {}", ecc.bt_payload());

        _sessions.for_each([&ecc](session::BaseSession& s) {
            // don't publish client contact to other end of outbound session
            if (s.is_outbound())
                return;
            log::debug(logcat, "Publishing ClientContact to remote on inbound session (remote:{})", s.remote());

            s.publish_client_contact(ecc, publish_cc_cb);
        });

        {
            Lock_t l{paths_mutex};

            for (const auto& [_, p] : _paths)
            {
                // If path-build is underway, don't use it
                if (not p or not p->is_active())
                    continue;

                log::debug(logcat, "Publishing ClientContact on {}", p->to_string());
                ret &= p->publish_client_contact(ecc, publish_cc_cb);
            }
        }

        return ret;
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
        sorted_intro_set intros,
        NetworkAddress remote,
        ClientIntro remote_intro,
        std::shared_ptr<path::Path> path,
        on_session_init_hook cb)
    {
        std::string inner_payload;
        shared_kx_data kx_data;
        auto pivot_txid = remote_intro.pivot_txid;
        intros.emplace(std::move(remote_intro));

        // internal payload for remote client
        std::tie(inner_payload, kx_data) = InitiateSession::serialize_encrypt(
            _router.local_rid(),
            remote.router_id(),
            path->pivot().txid(),
            pivot_txid,
            fetch_auth_token(remote),
            !_router.embedded());
        log::trace(logcat, "inner payload: {}", buffer_printer{inner_payload});

        auto intermediate_payload = PATH::CONTROL::serialize_aligned(as_bspan(inner_payload), pivot_txid);

        path->send_path_control_message(
            "path_control",
            as_bspan(intermediate_payload),
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
                            session->send_path_data_message(pkt.span());
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
        RemoteRC rc, NetworkAddress remote, std::shared_ptr<path::Path> path, on_session_init_hook cb)
    {
        auto pivot_txid = path->pivot().txid();
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
                            session->send_path_data_message(pkt.span());
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

    void SessionEndpoint::_make_relay_session_path(RemoteRC rc, NetworkAddress remote, on_session_init_hook cb)
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
        sorted_intro_set intros, NetworkAddress remote, on_session_init_hook cb)
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

    void SessionEndpoint::initiate_remote_session(NetworkAddress remote, on_session_init_hook cb)
    {
        _router.loop()->call([this, remote = std::move(remote), cb = std::move(cb)]() mutable {
            if (pending_sessions.contains(remote))
            {
                if (cb)
                    pending_session_hooks[remote].push_back(std::move(cb));
                log::debug(logcat, "Session init to remote {} already in progress.", remote);
                return;
            }

            pending_sessions[remote];
            pending_session_hooks[remote];

            if (remote.is_client())
                _initiate_client_session(remote, std::move(cb));
            else
                _initiate_relay_session(remote, std::move(cb));
        });
    }

    void SessionEndpoint::_initiate_client_session(NetworkAddress remote, on_session_init_hook cb)
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

    void SessionEndpoint::_initiate_relay_session(NetworkAddress remote, on_session_init_hook cb)
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

    void SessionEndpoint::map_remote_to_local_addr(NetworkAddress remote, quic::Address local)
    {
        _address_map.insert_or_assign(std::move(local), std::move(remote));
    }

    void SessionEndpoint::unmap_local_addr_by_remote(const NetworkAddress& remote) { _address_map.unmap(remote); }

    void SessionEndpoint::unmap_remote_by_name(const std::string& name) { _address_map.unmap(name); }

    bool SessionEndpoint::have_pending_session(const NetworkAddress& remote)
    {
        return pending_sessions.contains(remote);
    }

    void SessionEndpoint::queue_session_packet(const NetworkAddress& remote, IPPacket pkt)
    {
        if (pending_sessions.contains(remote))
        {
            if (pending_sessions[remote].size() < 100)  // FIXME: constant
                pending_sessions[remote].push_back(std::move(pkt));
        }
    }

}  //  namespace llarp::handlers
