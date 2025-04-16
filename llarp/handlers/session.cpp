#include "session.hpp"

#include <llarp/contact/contactdb.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/messages/fetch.hpp>
#include <llarp/messages/session.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/router/router.hpp>

#include <oxenc/base32z.h>

namespace llarp::handlers
{
    static auto logcat = log::Cat("SessionHandler");

    SessionEndpoint::SessionEndpoint(Router& r)
        : path::PathHandler{r, path::DEFAULT_PATHS_HELD, path::DEFAULT_LEN},
          _is_exit_node{_router.is_exit_node()},
          _is_service_node{_router.is_service_node()}
    {}

    const std::shared_ptr<EventLoop>& SessionEndpoint::loop() { return _router.loop(); }

    std::tuple<size_t, std::string, bool> SessionEndpoint::session_stats() const
    {
        return {_sessions.count(), _local_range.to_string(), _is_exit_node};
    }

    void SessionEndpoint::unmap_session(NetworkAddress remote, bool using_tun)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (using_tun)
            _router.tun_endpoint()->unmap_session_to_local_ip(remote);

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
            if (auto path = get_path_conditional(
                    [local_pivot_txid](std::shared_ptr<path::Path> p) { return p->pivot_txid() == local_pivot_txid; }))
            {
                log::debug(
                    logcat,
                    "Successfully matched path-switch request to InboundSession over path:{}",
                    (*path)->to_string());
                s->set_remote_pivot_tx(remote_pivot_txid);
                s->set_new_current_path_interface(std::move(*path));
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
                _sessions.for_each([](std::shared_ptr<session::BaseSession>& s) { s->send_path_close(); });
                prom.set_value();
            });

            prom.get_future().get();
            log::debug(logcat, "Dispatched all path close messages!");
        }

        _sessions.clear_sessions();

        path::PathHandler::stop();
    }

    void SessionEndpoint::configure()
    {
        auto net_config = _router.config()->network;

        if (_is_exit_node)
        {
            assert(not _is_service_node);

            _exit_policy = net_config.traffic_policy;
            client_contact.exit_policy = _exit_policy;
        }

        if (not net_config.srv_records.empty())
        {
            _srv_records.merge(net_config.srv_records);
            client_contact.SRVs = _srv_records;
        }

        if (_use_tokens = not net_config.auth_static_tokens.empty(); _use_tokens)
            _static_auth_tokens.merge(net_config.auth_static_tokens);

        if (_use_whitelist = not net_config.auth_whitelist.empty(); _use_whitelist)
            _auth_whitelist.merge(net_config.auth_whitelist);

        _if_name = *net_config._if_name;
        _local_range = *net_config._local_ip_range;
        _local_addr = *net_config._local_addr;
        _local_base_ip = *net_config._local_base_ip;

        _ipv6_enabled = net_config.enable_ipv6;

        // TESTNET: TODO: check if ipv6 is disabled
        for (auto& [addr, range] : net_config._exit_ranges)
        {
            _range_map.insert_or_assign(range, addr);
        }

        if (not net_config.exit_auths.empty())
        {
            _auth_tokens.merge(net_config.exit_auths);
        }

        // always accept ipv4 (currently)
        protoflags = meta::to_underlying(protocol_flag::IPV4);

        if (_ipv6_enabled)
            protoflags |= meta::to_underlying(protocol_flag::IPV6);

        // if we are a full client, we accept standard and tunneled (QUICTUN) traffic
        if (_router.using_tun_if())
            protoflags |= meta::to_underlying(protocol_flag::QUICTUN);

        if (_is_exit_node)
            protoflags |= meta::to_underlying(protocol_flag::EXIT);

        auto& key_manager = _router.key_manager();

        client_contact = ClientContact::generate(
            key_manager->derive_subkey(),
            key_manager->identity_data.to_pubkey(),
            _srv_records,
            protoflags,
            _exit_policy);

        should_publish_cc = net_config.is_reachable;
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

    void SessionEndpoint::path_rotation_succeeded(std::shared_ptr<path::Path> new_path)
    {
        log::info(logcat, "SessionEndpoint successfully rotated in new path: {}", new_path->to_string());
        // log::info(logcat, "SessionEndpoint successfully rotated in new path: {}", new_path->debug_string());
        path_build_succeeded(std::move(new_path));
        drop_oldest_path();
        update_and_publish_localcc();
    }

    std::optional<std::vector<RemoteRC>> SessionEndpoint::get_hops_to_random()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        auto filter = [this, &r = _router](const RemoteRC& rc) mutable {
            const auto& rid = rc.router_id();

            for (const auto& [_, p] : _paths)
            {
                if (p and p->pivot_rid() == rid)
                    return false;
            }

            return not r.router_profiling().is_bad_for_path(rid, 1);
        };

        if (auto maybe = _router.node_db()->get_random_rc_conditional(filter))
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

    void SessionEndpoint::srv_records_changed()
    {
        log::debug(logcat, "{} called", __PRETTY_FUNCTION__);
        update_and_publish_localcc(_srv_records);
    }

    void SessionEndpoint::start_tickers()
    {
        if (!_is_service_node and should_publish_cc)
        {
            log::trace(logcat, "Starting ClientContact publish ticker...");

            _router.loop()->call_later(approximate_time(5s, 5), [&]() {
                update_and_publish_localcc();
                _cc_publisher = _router.loop()->call_every(
                    CC_PUBLISH_INTERVAL,
                    [/* this */]() mutable {
                        log::critical(logcat, "TESTNET: Skipping ClientContact publish!");
                        // update_and_publish_localcc();
                    },
                    true);
            });

            log::trace(logcat, "Starting path rotation ticker...");
            _path_rotater =
                _router.loop()->call_every(path::PATH_ROTATION_INTERVAL, [this]() mutable { rotate_paths(); });
        }
        else
            log::info(logcat, "SessionEndpoint configured to NOT publish ClientContact...");
    }

    void SessionEndpoint::resolve_ons_mappings()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        auto& ons_ranges = _router.config()->network._ons_ranges;

        if (not ons_ranges.empty())
        {
            log::debug(logcat, "SessionEndpoint resolving {} SNS addresses mapped to IP ranges", ons_ranges.size());

            for (auto itr = ons_ranges.begin(); itr != ons_ranges.end();)
            {
                resolve_ons(
                    std::move(itr->first),
                    [this, ip_range = std::move(itr->second)](std::optional<NetworkAddress> maybe_addr) {
                        if (maybe_addr)
                        {
                            log::debug(
                                logcat,
                                "Successfully resolved SNS lookup for {} mapped to IPRange:{}",
                                *maybe_addr,
                                ip_range);
                            _range_map.insert_or_assign(std::move(ip_range), std::move(*maybe_addr));
                        }
                        // we don't need to print a fail message, as it is logged prior to invoking with std::nullopt
                    });

                itr = ons_ranges.erase(itr);
            }
        }

        auto& ons_auths = _router.config()->network.ons_exit_auths;

        if (auto n_ons_auths = ons_auths.size(); n_ons_auths > 0)
        {
            log::debug(logcat, "SessionEndpoint resolving {} ONS addresses mapped to auth tokens", n_ons_auths);

            for (auto itr = ons_auths.begin(); itr != ons_auths.end();)
            {
                resolve_ons(
                    std::move(itr->first),
                    [this, auth_token = std::move(itr->second)](std::optional<NetworkAddress> maybe_addr) {
                        if (maybe_addr)
                        {
                            log::debug(
                                logcat,
                                "Successfully resolved SNS lookup for {} mapped to static auth token",
                                *maybe_addr);
                            _auth_tokens.emplace(std::move(*maybe_addr), std::move(auth_token));
                        }
                        // we don't need to print a fail message, as it is logged prior to invoking with std::nullopt
                    });

                itr = ons_auths.erase(itr);
            }
        }
    }

    void SessionEndpoint::resolve_ons(std::string sns, std::function<void(std::optional<NetworkAddress>)> func)
    {
        if (not is_valid_sns(sns))
        {
            log::debug(logcat, "Invalid SNS name ({}) queried for lookup", sns);
            return func(std::nullopt);
        }

        log::debug(logcat, "Looking up SNS name {}", sns);

        auto response_handler = [sns_name = sns, hook = std::move(func)](oxen::quic::message m) mutable {
            try
            {
                if (m)
                {
                    log::debug(logcat, "Call to ResolveSNS succeeded!");

                    auto enc = ResolveSNS::deserialize_response(oxenc::bt_dict_consumer{m.body()});

                    if (auto client_addr = enc.decrypt(sns_name))
                    {
                        log::debug(
                            logcat,
                            "Successfully decrypted SNS record (name: {}, address: {})",
                            sns_name,
                            client_addr->to_string());
                        return hook(std::move(client_addr));
                    }

                    log::warning(logcat, "Failed to decrypt SNS record (name: {})", sns_name);
                }
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Exception: {}", e.what());
            }

            hook(std::nullopt);
        };

        {
            Lock_t l{paths_mutex};

            for (const auto& [_, path] : _paths)
            {
                log::info(
                    logcat, "Querying pivot:{} for name lookup (target: {})", path->pivot_rid().short_string(), sns);
                path->resolve_sns(sns, response_handler);
            }
        }
    }

    void SessionEndpoint::lookup_relay_contact(RouterID remote, std::function<void(std::optional<RemoteRC>)> func)
    {
        if (auto maybe_rc = _router.node_db()->get_rc(remote))
        {
            log::debug(logcat, "RelayContact for remote (rid: {}) found locally!", remote);
            return func(std::move(maybe_rc));
        }

        log::debug(logcat, "Looking up RelayContact for remote (rid:{})", remote.to_network_address(true));

        auto ignore_remaining = std::make_shared<std::atomic_bool>(false);

        auto response_handler =
            [this, remote, hook = std::move(func), ignore_remaining](oxen::quic::message m) mutable {
                if (ignore_remaining->load())
                {
                    log::trace(logcat, "Dropping subsequent `fetch_rc` response (success: {})...", not m.is_error());
                    return;
                }
                try
                {
                    if (m)
                    {
                        log::info(logcat, "Call to FetchRC succeeded!");
                        auto rcs = FetchRC::deserialize_response(oxenc::bt_dict_consumer{m.body()});

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
                        auto rc = rcs.extract(rcs.begin()).value();
                        _router.node_db()->put_rc(rc);
                        ignore_remaining->store(true);
                        return hook(std::move(rc));
                    }

                    std::optional<std::string> status = std::nullopt;
                    oxenc::bt_dict_consumer btdc{m.body()};

                    if (auto s = btdc.maybe<std::string>(messages::STATUS_KEY))
                        status = s;

                    log::warning(logcat, "Call to FetchRCs FAILED; reason: {}", status.value_or("<none given>"));
                }
                catch (const std::exception& e)
                {
                    log::warning(logcat, "Exception: {}", e.what());
                }

                hook(std::nullopt);
            };

        {
            Lock_t l{paths_mutex};

            for (const auto& [_, p] : _paths)
            {
                if (not p or not p->is_active())
                    continue;

                log::debug(
                    logcat,
                    "Querying pivot (rid:{}) for RelayContact lookup target (rid:{})",
                    p->pivot_rid().short_string(),
                    remote);

                p->fetch_relay_contact(remote, response_handler);
            }
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

        auto ignore_remaining = std::make_shared<std::atomic_bool>(false);

        auto response_handler =
            [this, remote, hook = std::move(func), ignore_remaining](oxen::quic::message m) mutable {
                if (ignore_remaining->load())
                {
                    log::trace(logcat, "Dropping subsequent `find_cc` response (success: {})...", not m.is_error());
                    return;
                }
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
                            ignore_remaining->store(true);
                            return hook(std::move(intro));
                        }

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

                hook(std::nullopt);
            };

        {
            Lock_t l{paths_mutex};

            for (const auto& [_, p] : _paths)
            {
                if (not p or not p->is_active())
                    continue;

                log::debug(
                    logcat,
                    "Querying pivot (rid:{}) for ClientContact lookup target (rid:{})",
                    p->pivot_rid().short_string(),
                    remote);

                p->find_client_contact(remote_key, response_handler);
            }
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
            client_contact.regenerate(std::move(intros));
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
        bool ret{true};

        if (_use_tokens)
            ret &= _static_auth_tokens.contains(*maybe_auth);

        if (_use_whitelist)
            ret &= _auth_whitelist.contains(remote);

        return ret;
    }

    static constexpr auto success_msg = "SessionEndpoint successfully created and mapped InboundSession object!"sv;

    std::optional<ip_v> SessionEndpoint::map_session(std::shared_ptr<session::BaseSession>& s)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (s->using_tun())
        {
            log::trace(logcat, "{} Instructing lokinet TUN device to create mapped route...", success_msg);

            if (auto maybe_ip = _router.tun_endpoint()->map_session_to_local_ip(s->remote()))
            {
                log::info(
                    logcat,
                    "TUN device successfully routing session (remote: {}) via local ip: {}",
                    s->remote(),
                    std::holds_alternative<ipv4>(*maybe_ip) ? std::get<ipv4>(*maybe_ip).to_string()
                                                            : std::get<ipv6>(*maybe_ip).to_string());
                return maybe_ip;
            }

            // TODO: if this fails, we should close the session
            log::warning(logcat, "TUN device failed to route session (remote: {}) to local ip", s->remote());
            return std::nullopt;
        }

        // TESTNET:
        log::warning(logcat, "INCOMPLETE EMBEDDED ROUTE");
        // log::info(logcat, "{} Connecting to TCP backend to route session traffic...", success_msg);
        // session->tcp_backend_connect();

        return std::nullopt;
    }

    std::optional<session_tag> SessionEndpoint::prefigure_session(
        NetworkAddress initiator,
        HopID remote_pivot_txid,
        std::shared_ptr<session_path_interface> path,
        shared_kx_data kx_data,
        bool use_tun)
    {
        auto tag = session_tag::make(protoflags);

        std::shared_ptr<session::BaseSession> s = nullptr;

        if (_is_service_node)
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

        if (auto maybe_ip = map_session(s))
            return tag;

        return std::nullopt;
    }

    static void publish_cc_cb(oxen::quic::message m)
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

        _sessions.for_each([ecc](std::shared_ptr<session::BaseSession>& s) mutable {
            log::debug(
                logcat,
                "Publishing ClientContact on {}bound session (remote:{})",
                detail::bool_alpha(s->is_outbound(), "Out", "In"),
                s->remote());

            s->publish_client_contact(ecc, publish_cc_cb);
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
        intro_set intros,
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
            path->pivot_txid(),
            pivot_txid,
            fetch_auth_token(remote),
            _router.using_tun_if());
        log::trace(logcat, "inner payload: {}", buffer_printer{inner_payload});

        auto intermediate_payload = PATH::CONTROL::serialize_aligned(std::move(inner_payload), pivot_txid);

        path->send_path_control_message(
            "path_control",
            std::move(intermediate_payload),
            [this,
             remote,
             path,
             remote_pivot_txid = pivot_txid,
             remote_intros = std::move(intros),
             hook = std::move(cb),
             session_keys = std::move(kx_data)](oxen::quic::message m) mutable {
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

                    // TESTNET: use new ::map_session(...) function after finishing embedded hooks
                    if (session->using_tun())
                    {
                        log::trace(logcat, "Instructing lokinet TUN device to create mapped route...");
                        if (auto maybe_ip = _router.tun_endpoint()->map_session_to_local_ip(session->remote()))
                        {
                            log::info(
                                logcat,
                                "TUN device successfully routing session (remote: {}) via local ip: {}",
                                session->remote(),
                                std::holds_alternative<ipv4>(*maybe_ip) ? std::get<ipv4>(*maybe_ip).to_string()
                                                                        : std::get<ipv6>(*maybe_ip).to_string());

                            return hook(*maybe_ip);
                        }

                        log::critical(
                            logcat,
                            "Lokinet TUN failed to map route for session traffic to remote: {}",
                            session->remote());
                        // TESTNET: TODO: CLOSE THIS HERE
                    }
                    else
                    {
                        log::info(logcat, "Starting TCP listener to route session traffic to backend...");
                        session->tcp_backend_listen(std::move(hook));
                    }
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
                }
            });

        log::debug(logcat, "message sent...");
    }

    void SessionEndpoint::_make_relay_session(
        RemoteRC rc, NetworkAddress remote, std::shared_ptr<path::Path> path, on_session_init_hook cb)
    {
        auto pivot_txid = path->pivot_txid();
        std::string payload = InitiateSession::serialize(
            _router.local_rid(), pivot_txid, pivot_txid, fetch_auth_token(remote), _router.using_tun_if());

        log::trace(logcat, "payload: {}", buffer_printer{payload});

        path->send_path_control_message(
            "session_init",
            std::move(payload),
            [this,
             rc = std::move(rc),
             remote,
             path,
             pivot_txid,
             hook = std::move(cb),
             session_keys = path->hops.back().kx](oxen::quic::message m) mutable {
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

                    if (session->using_tun())
                    {
                        log::trace(logcat, "Instructing lokinet TUN device to create mapped route...");
                        if (auto maybe_ip = _router.tun_endpoint()->map_session_to_local_ip(session->remote()))
                        {
                            log::info(
                                logcat,
                                "TUN device successfully routing session (remote: {}) via local ip: {}",
                                session->remote(),
                                std::holds_alternative<ipv4>(*maybe_ip) ? std::get<ipv4>(*maybe_ip).to_string()
                                                                        : std::get<ipv6>(*maybe_ip).to_string());

                            return hook(*maybe_ip);
                        }

                        log::critical(
                            logcat,
                            "Lokinet TUN failed to map route for session traffic to remote: {}",
                            session->remote());
                    }
                    else
                    {
                        log::info(logcat, "Starting TCP listener to route session traffic to backend...");
                        session->tcp_backend_listen(std::move(hook));
                    }
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

    void SessionEndpoint::_make_client_session_path(intro_set intros, NetworkAddress remote, on_session_init_hook cb)
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

    void SessionEndpoint::initiate_remote_session(const NetworkAddress& remote, on_session_init_hook cb)
    {
        if (remote.is_client())
            _initiate_client_session(remote, std::move(cb));
        else
            _initiate_relay_session(remote, std::move(cb));
    }

    void SessionEndpoint::_initiate_client_session(NetworkAddress remote, on_session_init_hook cb)
    {
        auto counter = std::make_shared<size_t>(num_paths_desired);

        _router.loop()->call([this, remote, handler = std::move(cb), counter]() mutable {
            lookup_client_intro(
                remote.router_id(),
                [this, remote, hook = std::move(handler), counter](std::optional<ClientContact> cc) mutable {
                    if (*counter == 0)
                        return;

                    if (cc)
                    {
                        *counter = 0;
                        log::debug(logcat, "Session initiation returned client contact: {}", cc->to_string());
                        _make_client_session_path(std::move(*cc).take_intros(), remote, std::move(hook));
                    }
                    else if (--*counter == 0)
                        log::warning(logcat, "Failed to initiate session at 'find_cc' (target:{})", remote);
                });
        });
    }

    void SessionEndpoint::_initiate_relay_session(NetworkAddress remote, on_session_init_hook cb)
    {
        auto counter = std::make_shared<size_t>(num_paths_desired);

        _router.loop()->call([this, remote, handler = std::move(cb), counter]() mutable {
            lookup_relay_contact(
                remote.router_id(),
                [this, remote, hook = std::move(handler), counter](std::optional<RemoteRC> rc) mutable {
                    if (*counter == 0)
                        return;

                    if (rc)
                    {
                        *counter = 0;
                        log::debug(logcat, "Session initiation returned RC: {}", rc->to_string());
                        _make_relay_session_path(std::move(*rc), remote, std::move(hook));
                    }
                    else if (--*counter == 0)
                        log::warning(logcat, "Failed to initiate session at `fetch_rcs` (target:{})", remote);
                });
        });
    }

    void SessionEndpoint::map_remote_to_local_addr(NetworkAddress remote, oxen::quic::Address local)
    {
        _address_map.insert_or_assign(std::move(local), std::move(remote));
    }

    void SessionEndpoint::unmap_local_addr_by_remote(const NetworkAddress& remote) { _address_map.unmap(remote); }

    void SessionEndpoint::unmap_remote_by_name(const std::string& name) { _address_map.unmap(name); }

    void SessionEndpoint::map_remote_to_local_range(NetworkAddress remote, IPRange range)
    {
        _range_map.insert_or_assign(std::move(range), std::move(remote));
    }

    void SessionEndpoint::unmap_local_range_by_remote(const NetworkAddress& remote) { _range_map.unmap(remote); }

    void SessionEndpoint::unmap_range_by_name(const std::string& name) { _range_map.unmap(name); }

}  //  namespace llarp::handlers
