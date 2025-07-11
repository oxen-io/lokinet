#include "link_manager.hpp"

#include <llarp/contact/contactdb.hpp>
#include <llarp/contact/router_id.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/messages/fetch.hpp>
#include <llarp/messages/path.hpp>
#include <llarp/messages/session.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/path/path.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/bspan.hpp>

#ifndef LOKINET_EMBEDDED_ONLY
#include <llarp/rpc/rpc_client.hpp>
#endif

#include <oxen/quic/context.hpp>
#include <oxenc/bt_producer.h>
#include <sodium/crypto_generichash_blake2b.h>
#include <sodium/randombytes.h>

#include <algorithm>
#include <exception>
#include <set>

namespace llarp
{
    static auto logcat = llarp::log::Cat("link_manager");

    static std::vector<uint8_t> make_static_secret(
        const Ed25519SecretKey& sk, std::string_view static_secret_key = "Lokinet static shared secret key"sv)
    {
        std::vector<uint8_t> secret;
        secret.resize(32);

        crypto_generichash_blake2b_state st;
        crypto_generichash_blake2b_init(
            &st, reinterpret_cast<const uint8_t*>(static_secret_key.data()), static_secret_key.size(), secret.size());
        crypto_generichash_blake2b_update(&st, sk.data(), sk.size());
        crypto_generichash_blake2b_final(&st, secret.data(), secret.size());

        return secret;
    }

    namespace link
    {
        Endpoint::Endpoint(std::shared_ptr<quic::Endpoint> ep, LinkManager& lm)
            : endpoint{std::move(ep)}, link_manager{lm}, _is_service_node{link_manager.is_service_node()}
        {}

        std::shared_ptr<link::Connection> Endpoint::get_service_conn(const RouterID& remote) const
        {
            return link_manager.router().loop()->call_get([this, rid = remote]() -> std::shared_ptr<link::Connection> {
                if (auto itr = service_conns.find(rid); itr != service_conns.end())
                    return itr->second;

                return nullptr;
            });
        }

        std::shared_ptr<link::Connection> Endpoint::get_conn(const RouterID& remote) const
        {
            if (auto itr = service_conns.find(remote); itr != service_conns.end())
                return itr->second;

            if (_is_service_node)
            {
                if (auto itr = client_conns.find(remote); itr != client_conns.end())
                    return itr->second;
            }

            return nullptr;
        }

        bool Endpoint::have_conn(const RouterID& remote) const
        {
            return have_service_conn(remote) or have_client_conn(remote);
        }

        bool Endpoint::have_client_conn(const RouterID& remote) const
        {
            return link_manager.router().loop()->call_get([this, remote]() { return client_conns.count(remote); });
        }

        bool Endpoint::have_service_conn(const RouterID& remote) const
        {
            return link_manager.router().loop()->call_get([this, remote]() { return service_conns.count(remote); });
        }

        void Endpoint::for_each_service_conn(
            std::function<void(RouterID, std::shared_ptr<link::Connection>)> func, bool active_only)
        {
            assert(link_manager.router().loop()->inside());

            std::ranges::for_each(service_conns.begin(), service_conns.end(), [&](auto c) mutable {
                if (c.second and (active_only ? c.second->is_active.load() : true))
                    func(c.first, c.second);
            });
        }

        void Endpoint::for_each_connection(std::function<void(const RouterID&, link::Connection&)> func)
        {
            link_manager.router().loop()->call([this, func = std::move(func)]() mutable {
                for (auto& [rid, conn] : service_conns)
                    if (conn)
                        func(rid, *conn);

                if (_is_service_node)
                {
                    for (auto& [rid, conn] : client_conns)
                        if (conn)
                            func(rid, *conn);
                }
            });
        }

        void Endpoint::close_connection(RouterID _rid)
        {
            link_manager._router.loop()->call([this, rid = _rid]() {
                if (auto itr = service_conns.find(rid); itr != service_conns.end())
                {
                    log::info(logcat, "Closing connection to relay RID:{}", rid);
                    auto& conn = *itr->second->conn;
                    conn.close_connection();
                }
                else if (_is_service_node)
                {
                    if (auto itr = client_conns.find(rid); itr != client_conns.end())
                    {
                        log::info(logcat, "Closing connection to client RID:{}", rid);
                        auto& conn = *itr->second->conn;
                        conn.close_connection();
                    }
                }
                else
                    log::warning(logcat, "Could not find connection to RID:{} to close!", rid);
            });
        }

        void Endpoint::close_all()
        {
            for (auto& conn : service_conns)
                conn.second->close_quietly();

            service_conns.clear();

            for (auto& conn : client_conns)
                conn.second->close_quietly();

            client_conns.clear();
        }

        std::tuple<size_t, size_t, size_t, size_t> Endpoint::connection_stats() const
        {
            return link_manager.router().loop()->call_get([this]() -> std::tuple<size_t, size_t, size_t, size_t> {
                size_t in{0}, out{0};

                for (const auto& [_, c] : service_conns)
                {
                    if (not c)
                        continue;

                    if (c->is_inbound())
                        ++in;
                    else
                        ++out;
                }

                for (const auto& [_, c] : client_conns)
                {
                    if (not c)
                        continue;

                    if (c->is_inbound())
                        ++in;
                    else
                        ++out;
                }

                return {in, out, service_conns.size(), client_conns.size()};
            });
        }

        size_t Endpoint::num_client_conns() const
        {
            return link_manager.router().loop()->call_get([this]() { return client_conns.size(); });
        }

        size_t Endpoint::num_router_conns(bool active_only) const
        {
            return link_manager.router().loop()->call_get([&]() {
                size_t n{};

                for (const auto& [_, conn] : service_conns)
                    if (conn and (active_only ? conn->is_active.load() : true))
                        ++n;

                return n;
            });
        }

        bool Endpoint::establish_and_send_control(RemoteRC rc, std::function<void(quic::BTRequestStream&)> send_hook)
        {
            log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
            return link_manager.router().loop()->call_get([&]() {
                auto rid = rc.router_id();

                try
                {
                    auto [itr, b] = service_conns.try_emplace(rid, nullptr);

                    if (not b)
                    {
                        log::debug(logcat, "Attempting to establish an already existing connection!");
                        send_hook(*itr->second->control_stream);
                        return true;
                    }

                    auto conn = endpoint->connect(
                        quic::RemoteAddress{rid.to_view(), rc.addr()},
                        link_manager.tls_creds,
                        quic::opt::keep_alive{_is_service_node ? RELAY_KEEP_ALIVE : CLIENT_KEEP_ALIVE},
                        [this, itr = std::move(itr), rid, send_hook = std::move(send_hook)](
                            quic::Connection& conn) mutable {
                            log::debug(
                                logcat,
                                "{} batch dispatching to remote (rid:{})",
                                _is_service_node ? "Relay" : "Client",
                                rid.short_string());
                            send_hook(*itr->second->control_stream);
                            link_manager.on_conn_open(conn);
                        });

                    auto control_stream = link_manager.make_control(*conn, rid);

                    itr->second = std::make_shared<link::Connection>(std::move(conn), std::move(control_stream));

                    log::info(logcat, "Outbound connection to RID:{} added to service conns...", rid.short_string());
                    return true;
                }
                catch (const std::exception& e)
                {
                    log::error(
                        logcat, "Exception caught establishing connection to {}: {}", rid.short_string(), e.what());
                    return false;
                }
                return true;
            });
        }
    }  // namespace link

    // These requests come over a path (as a "path_control" request),
    // we may or may not need to make a request to another relay,
    // then respond (onioned) back along the path.
    std::unordered_map<std::string_view, void (LinkManager::*)(quic::message, std::optional<std::string>)>
        LinkManager::path_requests = {
            {"path_control"sv, &LinkManager::_handle_path_control},
            {"publish_cc"sv, &LinkManager::_handle_publish_cc},
            {"find_cc"sv, &LinkManager::_handle_find_cc},
            {"fetch_rcs"sv, &LinkManager::_handle_fetch_rcs},
            {"resolve_sns"sv, &LinkManager::_handle_resolve_sns},
            {"session_init"sv, &LinkManager::_handle_initiate_session},
            {"session_close"sv, &LinkManager::_handle_close_session},
            {"path_switch"sv, &LinkManager::_handle_path_switch},
            {"path_ping"sv, &LinkManager::_handle_path_ping}};

    std::tuple<size_t, size_t, size_t, size_t> LinkManager::connection_stats() const { return ep->connection_stats(); }

    size_t LinkManager::get_num_connected_routers(bool active_only) const { return ep->num_router_conns(active_only); }

    size_t LinkManager::get_num_connected_clients() const { return ep->num_client_conns(); }

    std::unordered_set<RouterID> LinkManager::get_current_remotes() const
    {
        // invoke using Router method to wrap in call_get
        std::unordered_set<RouterID> ret;

        for (auto& [rid, conn] : ep->service_conns)
            if (conn and conn->is_active)
                ret.insert(rid);

        return ret;
    }

    void LinkManager::for_each_connection(std::function<void(const RouterID&, link::Connection&)> func)
    {
        if (is_stopping)
            return;

        return ep->for_each_connection(std::move(func));
    }

    void LinkManager::register_commands(quic::BTRequestStream& s, const RouterID& remote_rid, bool client_only)
    {
        // TODO FIXME: registering all these commands on every stream feels icky; a quic fallback
        // handler could do this better.

        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        s.register_handler("path_control"s, [this](quic::message m) mutable {
            _router.loop()->call([&, msg = std::move(m)]() mutable { handle_path_control(std::move(msg)); });
        });

        if (client_only)
        {
            log::trace(logcat, "Registered all client-only BTStream commands!");
            return;
        }

        s.register_handler("path_switch"s, [this](quic::message m) {
            _router.loop()->call([this, msg = std::move(m)]() mutable { handle_path_switch(std::move(msg)); });
        });

        s.register_handler("session_init"s, [this](quic::message m) {
            _router.loop()->call([this, msg = std::move(m)]() mutable { handle_initiate_session(std::move(msg)); });
        });

        s.register_handler("session_close"s, [this](quic::message m) {
            _router.loop()->call([this, msg = std::move(m)]() mutable { handle_close_session(std::move(msg)); });
        });

        s.register_handler("path_build"s, [this, remote_rid](quic::message m) {
            _router.loop()->call(
                [this, remote_rid, msg = std::move(m)]() mutable { handle_path_build(std::move(msg), remote_rid); });
        });

        s.register_handler("bfetch_rcs"s, [this](quic::message m) {
            _router.loop()->call([this, msg = std::move(m)]() mutable { handle_fetch_bootstrap_rcs(std::move(msg)); });
        });

        s.register_handler("fetch_rcs"s, [this](quic::message m) {
            _router.loop()->call([this, msg = std::move(m)]() mutable { _handle_fetch_rcs(std::move(msg)); });
        });

        s.register_handler("fetch_rids"s, [this](quic::message m) {
            _router.loop()->call([this, msg = std::move(m)]() mutable { handle_fetch_router_ids(std::move(msg)); });
        });

        s.register_handler("gossip_rc"s, [this](quic::message m) {
            _router.loop()->call([this, msg = std::move(m)]() mutable { handle_gossip_rc(std::move(msg)); });
        });

        s.register_handler("publish_cc"s, [this](quic::message m) {
            _router.loop()->call([this, msg = std::move(m)]() mutable { _handle_publish_cc(std::move(msg)); });
        });

        s.register_handler("find_cc"s, [this](quic::message m) {
            _router.loop()->call([this, msg = std::move(m)]() mutable { _handle_find_cc(std::move(msg)); });
        });

        s.register_handler("resolve_sns"s, [this](quic::message m) {
            _router.loop()->call([this, msg = std::move(m)]() mutable { _handle_resolve_sns(std::move(msg)); });
        });

        log::trace(logcat, "Registered all commands for connection to remote RID:{}", remote_rid);
    }

    void LinkManager::start_tickers()
    {
        log::debug(logcat, "Starting gossip ticker...");

        _router.loop()->call_later(uniform_duration_distribution{5s, 10s}(llarp::csrng), [this] {
            regenerate_and_gossip_rc();
            _gossip_ticker =
                _router.loop()->call_every(_router.gossip_interval(), [this] { regenerate_and_gossip_rc(); });
        });
    }

    LinkManager::LinkManager(Router& r)
        : _router{r},
          _is_service_node{_router.is_service_node()},
          quic_loop{std::make_unique<quic::Loop>()},
          tls_creds{quic::GNUTLSCreds::make_from_ed_keys(
              {reinterpret_cast<const char*>(_router.identity().data()), 32},
              {reinterpret_cast<const char*>(_router.local_rid().data()), 32})},
          ep{std::make_unique<link::Endpoint>(startup_endpoint(), *this)},
          is_stopping{false}
    {}

    std::shared_ptr<quic::Endpoint> LinkManager::startup_endpoint()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        /** Parameters:
              - local bind address
              - conection open callback
              - connection close callback
              - stream constructor callback
                - will return a BTRequestStream on the first call to get_new_stream<BTRequestStream>
                - bt stream construction contains a stream close callback that shuts down the
                    connection if the btstream closes unexpectedly
        */

        std::optional<quic::opt::inbound_alpns> inbound_alpn;
        if (is_service_node())
            inbound_alpn.emplace({RELAY_ALPN, CLIENT_ALPN});

        auto e = quic::Endpoint::endpoint(
            *quic_loop,
            _router.listen_addr(),
            quic::opt::static_secret{make_static_secret(_router.identity())},
            [this](quic::Connection& conn) { return on_conn_open(conn); },
            [this](quic::Connection& conn, uint64_t ec) { return on_conn_closed(conn, ec); },
            [this](quic::datagram dgram) { return handle_path_data_message(std::move(dgram)); },
            inbound_alpn,
            quic::opt::outbound_alpns{{is_service_node() ? RELAY_ALPN : CLIENT_ALPN}},
            quic::opt::enable_datagrams{quic::Splitting::ACTIVE});

        if (_router.is_service_node())
            tls_creds->set_key_verify_callback([this](const std::span<const uint8_t> key, const std::string_view alpn) {
                assert(is_service_node());

                bool incoming_client = alpn == CLIENT_ALPN;
                if (!incoming_client && alpn != RELAY_ALPN)
                {
                    log::warning(logcat, "Rejecting incoming connection with unknown ALPN {}", alpn);
                    return false;
                }

                if (key.size() != RouterID::SIZE)
                {
                    log::warning(
                        logcat,
                        "Rejecting incoming connection with invalid/unsupported pubkey ({} bytes, expected {})",
                        key.size(),
                        RouterID::SIZE);
                    return false;
                }

                return _router.loop()->call_get([this, incoming_client, other = RouterID{key.first<32>()}] {
                    if (incoming_client)
                    {
                        // FIXME: what if the client is reconnecting but we already have something
                        // in ep->client_conns?
                        log::debug(logcat, "Accepting client connection from {}!", other);
                        ep->client_conns.emplace(other, nullptr);
                        return true;
                    }

                    // verify incoming service node
                    if (not _router.node_db().registered_routers().contains(other))
                    {
                        log::warning(
                            logcat, "Rejecting incoming relay connection from unregistered relay (RID:{})", other);
                        return false;
                    }

                    auto [itr, b] = ep->service_conns.emplace(other, nullptr);
                    if (b)
                    {
                        log::debug(logcat, "Accepting inbound from registered relay (RID:{})", other);
                        return true;
                    }

                    // If we fail to emplace a connection to the incoming RID, then we are
                    // simultaneously dealing with an outbound and inbound with the same remote.  To
                    // resolve this consistently on both ends, both relays will defer to the
                    // connection initiated by the RID that appears first in lexicographical order.
                    auto defer_to_incoming = other < router().local_rid();

                    if (defer_to_incoming)
                    {
                        if (itr->second)
                            itr->second->conn->set_close_quietly();
                        itr->second = nullptr;
                    }

                    log::debug(
                        logcat,
                        "Received inbound with ongoing outbound to relay (RID:{}); {}!",
                        other,
                        defer_to_incoming ? "deferring to inbound" : "rejecting in favor of outbound");

                    return defer_to_incoming;
                });
            });

        e->listen(tls_creds);

        return e;
    }

    std::shared_ptr<quic::BTRequestStream> LinkManager::make_control(quic::Connection& conn, const RouterID& remote)
    {
        std::shared_ptr<quic::BTRequestStream> control_stream;

        if (conn.is_inbound())
        {
            control_stream =
                conn.template queue_incoming_stream<quic::BTRequestStream>([](quic::Stream&, uint64_t error_code) {
                    log::warning(logcat, "BTRequestStream closed unexpectedly (ec:{})", error_code);
                });

            log::trace(logcat, "Queued BTStream to be opened (ID:{})", control_stream->stream_id());
            assert(control_stream->stream_id() == 0);
        }
        else
        {
            control_stream = conn.open_stream<quic::BTRequestStream>([](quic::Stream&, uint64_t error_code) {
                log::warning(logcat, "BTRequestStream closed unexpectedly (ec:{})", error_code);
            });

            log::trace(logcat, "Opened BTStream (ID:{})", control_stream->stream_id());
        }

        register_commands(*control_stream, remote, not _is_service_node);
        return control_stream;
    }

    void LinkManager::on_inbound_conn(std::shared_ptr<quic::Connection> conn)
    {
        assert(_is_service_node);
        assert(conn->remote_key().size() == RouterID::SIZE);  // Should have been checked in the key verify callback
        RouterID rid{conn->remote_key().first<RouterID::SIZE>()};

        auto control = make_control(*conn, rid);
        bool is_client_conn = false;

        if (auto it = ep->service_conns.find(rid); it != ep->service_conns.end())
        {
            log::debug(logcat, "Configuring inbound connection from relay RID:{}", rid.short_string());
            it->second = std::make_shared<link::Connection>(std::move(conn), std::move(control), false, true);
        }
        else if (auto it = ep->client_conns.find(rid); it != ep->client_conns.end())
        {
            is_client_conn = true;
            log::debug(logcat, "Configuring inbound connection from client RID:{}", rid.short_string());
            it->second = std::make_shared<link::Connection>(std::move(conn), std::move(control), false, true);
        }
        else
            log::warning(logcat, "Could not find inbound connection corresponding to RID: {}", rid);

        log::critical(
            logcat,
            "SERVICE NODE (RID:{}) ESTABLISHED CONNECTION TO RID:{}",
            _router.local_rid().to_network_address(),
            rid.to_network_address(!is_client_conn));
    }

    void LinkManager::on_outbound_conn(RouterID rid)
    {
        log::trace(logcat, "Outbound connection to {}", rid);

        if (auto conn = ep->get_service_conn(rid))
        {
            conn->is_active = true;
            log::trace(logcat, "Fetched configured outbound connection to relay RID: {}", rid);
        }
        else
        {
            log::warning(logcat, "Could not find outbound connection corresponding to RID: {}", rid);

            log::critical(
                logcat,
                "{} (RID:{}) ESTABLISHED CONNECTION TO RID:{}",
                _is_service_node ? "SERVICE NODE" : "CLIENT",
                _router.local_rid().to_network_address(_is_service_node),
                rid.to_network_address(/*is_relay=*/true));
        }
    }

    void LinkManager::on_conn_open(quic::Connection& conn)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        _router.loop()->call([this, wci = conn.weak_from_this()]() {
            auto conn = wci.lock();

            if (not conn)
            {
                log::warning(logcat, "Connection died before connection open callback execution!");
                return;
            }

            if (conn->is_inbound())
                on_inbound_conn(std::move(conn));
            else
            {
                auto key = conn->remote_key();
                assert(key.size() == RouterID::SIZE);
                on_outbound_conn(RouterID{key.first<RouterID::SIZE>()});
            }
        });
    }

    void LinkManager::on_conn_closed(quic::Connection& conn, uint64_t ec)
    {
        if (!conn.remote_key().size())
        {
            log::debug(logcat, "on_conn_closed on rejected connection, nothing to do");
            return;
        }
        assert(conn.remote_key().size() == 32);

        _router.loop()->call([this,
                              ref_id = conn.reference_id(),
                              rid = RouterID{conn.remote_key().first<32>()},
                              error_code = ec,
                              path = conn.path()]() {
            log::debug(logcat, "Purging quic connection {} (ec:{}) path:{}", ref_id, error_code, path);

            if (auto s_itr = ep->service_conns.find(rid); s_itr != ep->service_conns.end())
            {
                log::debug(logcat, "Quic connection to relay RID:{} purged successfully", rid.short_string());
                ep->service_conns.erase(s_itr);
            }
            else if (auto c_itr = ep->client_conns.find(rid); c_itr != ep->client_conns.end())
            {
                log::debug(logcat, "Quic connection to client RID:{} purged successfully", rid.short_string());
                ep->client_conns.erase(c_itr);
            }
            else
                log::trace(logcat, "Nothing to purge for quic connection {}", ref_id);
        });
    }

    bool LinkManager::send_control_message(
        const RouterID& remote, std::string endpoint, std::string body, std::function<void(quic::message)> func)
    {
        if (is_stopping)
            return false;

        if (func)
        {
            func = [this, f = std::move(func)](quic::message m) mutable {
                _router.loop()->call([func = std::move(f), msg = std::move(m)]() mutable { func(std::move(msg)); });
            };
        }

        if (auto conn = ep->get_conn(remote); conn)
        {
            conn->control_stream->command(std::move(endpoint), std::move(body), std::move(func));
            return true;
        }

        log::debug(logcat, "Queueing control message to {}", remote);

        _router.loop()->call([this,
                              rid = remote,
                              endpoint = std::move(endpoint),
                              body = std::move(body),
                              f = std::move(func)]() mutable {
            connect_and_send(std::move(rid), std::move(endpoint), std::move(body), std::move(f));
        });

        return false;
    }

    bool LinkManager::send_data_message(const RouterID& remote, std::string body)
    {
        if (is_stopping)
            return false;

        if (auto conn = ep->get_conn(remote); conn)
        {
            conn->datagrams->send(std::move(body));
            return true;
        }

        log::debug(logcat, "Queueing data message to {}", remote);

        _router.loop()->call([this, body = std::move(body), rid = remote]() {
            connect_and_send(std::move(rid), std::nullopt, std::move(body));
        });

        return false;
    }

    void LinkManager::close_connection(RouterID rid) { return ep->close_connection(rid); }

    void LinkManager::test_reachability(
        const RouterID& rid, connection_established_callback on_open, connection_closed_callback on_close)
    {
        if (auto rc = _router.node_db().get_rc(rid))
        {
            connect_to(*rc, std::move(on_open), std::move(on_close));
        }
        else
            log::warning(logcat, "Could not find RelayContact for connection to rid:{}", rid);
    }

    void LinkManager::connect_and_send(const RouterID& router, std::function<void(quic::BTRequestStream&)> send_hook)
    {
        if (auto rc = _router.node_db().get_rc(router))
        {
            if (ep->establish_and_send_control(*rc, std::move(send_hook)))
                log::info(logcat, "Begun establishing connection to {}", router);
            else
                log::warning(logcat, "Failed to begin establishing connection to {}", router);
        }
        else
            log::error(logcat, "Error: Could not find RC for connection to rid:{}, message not sent!", router);
    }

    bool link::Endpoint::establish_and_send(
        quic::RemoteAddress remote,
        RouterID rid,
        std::optional<std::string> ep,
        std::string body,
        std::function<void(quic::message)> func)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        return link_manager.router().loop()->call_get([&]() {
            try
            {
                const auto& is_control = ep.has_value();

                log::debug(logcat, "Establishing connection to RID:{}", rid);

                // add to service conns
                auto [itr, b] = service_conns.try_emplace(rid, nullptr);

                if (not b)
                {
                    log::debug(logcat, "ERROR: attempting to establish an already-existing connection");
                    (is_control)
                        ? itr->second->control_stream->command(std::move(*ep), std::move(body), std::move(func))
                        : itr->second->conn->datagrams()->send(std::move(body));
                    return true;
                }

                auto conn_ = endpoint->connect(
                    remote,
                    link_manager.tls_creds,
                    quic::opt::keep_alive{_is_service_node ? RELAY_KEEP_ALIVE : CLIENT_KEEP_ALIVE},
                    [this, itr, rid, ep = std::move(ep), body = std::move(body), func = std::move(func)](
                        quic::Connection& conn) mutable {
                        auto& control_stream = itr->second->control_stream;
                        log::trace(
                            logcat,
                            "{} dispatching {} on outbound connection to remote (rid:{})",
                            _is_service_node ? "Relay" : "Client",
                            ep.has_value() ? "control message (ep:{})"_format(*ep) : "data message",
                            rid);

                        (ep.has_value()) ? control_stream->command(std::move(*ep), std::move(body), std::move(func))
                                         : conn.datagrams()->send(std::move(body));
                        link_manager.on_conn_open(conn);
                    });

                auto control_stream = link_manager.make_control(*conn_, rid);

                itr->second = std::make_shared<link::Connection>(std::move(conn_), std::move(control_stream));

                log::trace(logcat, "Outbound connection to RID:{} added to service conns...", rid);
                return true;
            }
            catch (const std::exception& e)
            {
                log::error(logcat, "Exception caught establishing connection to {}: {}", remote, e.what());
                return false;
            }
        });
    }

    void LinkManager::connect_and_send(
        const RouterID& router,
        std::optional<std::string> endpoint,
        std::string body,
        std::function<void(quic::message)> func)
    {
        // by the time we have called this, we have already checked if we have a connection to this
        // RID in ::send_control_message, at which point we will dispatch on that stream
        if (auto rc = _router.node_db().get_rc(router))
        {
            const auto& remote_addr = rc->addr();

            if (auto rv = ep->establish_and_send(
                    quic::RemoteAddress{router.to_view(), remote_addr},
                    router,
                    std::move(endpoint),
                    std::move(body),
                    std::move(func));
                rv)
            {
                log::debug(logcat, "Begun establishing connection to {}", remote_addr);
                return;
            }

            log::warning(logcat, "Failed to begin establishing connection to {}", remote_addr);
        }
        else
            log::error(logcat, "Error: Could not find RC for connection to rid:{}, message not sent!", router);
    }

    bool link::Endpoint::establish_connection(
        quic::RemoteAddress remote,
        RouterID rid,
        connection_established_callback on_open,
        connection_closed_callback on_close)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        return link_manager.router().loop()->call_get([&]() {
            try
            {
                log::debug(logcat, "Establishing connection to RID:{}", rid.short_string());
                // add to service conns
                auto [itr, b] = service_conns.try_emplace(rid, nullptr);

                if (not b)
                {
                    log::debug(logcat, "ERROR: attempting to establish an already-existing connection");
                    return b;
                }

                auto conn_ = endpoint->connect(
                    remote,
                    link_manager.tls_creds,
                    quic::opt::keep_alive{_is_service_node ? RELAY_KEEP_ALIVE : CLIENT_KEEP_ALIVE},
                    std::move(on_open),
                    std::move(on_close));

                log::trace(logcat, "Created outbound connection with path: {}", conn_->path());

                auto control_stream =
                    conn_->template open_stream<quic::BTRequestStream>([](quic::Stream&, uint64_t error_code) {
                        log::warning(logcat, "BTRequestStream closed unexpectedly (ec:{})", error_code);
                    });

                link_manager.register_commands(*control_stream, rid, not _is_service_node);

                itr->second = std::make_shared<link::Connection>(std::move(conn_), std::move(control_stream));

                log::trace(logcat, "Outbound connection to RID:{} added to service conns...", rid.short_string());
                return true;
            }
            catch (...)
            {
                log::error(logcat, "Error: failed to establish connection to {}", remote);
                return false;
            }
        });
    }

    void LinkManager::connect_to(
        const RemoteRC& rc, connection_established_callback on_open, connection_closed_callback on_close)
    {
        auto rid = rc.router_id();

        if (ep->have_service_conn(rid))
        {
            log::warning(logcat, "We already have a connection to {}!", rid);
            // TODO: should implement some connection failed logic, but not the same logic that
            // would be executed for another failure case
            return;
        }

        auto remote_addr = rc.addr();

        if (auto rv = ep->establish_connection(
                quic::RemoteAddress{rid.to_view(), remote_addr}, rid, std::move(on_open), std::move(on_close));
            rv)
        {
            log::debug(logcat, "Begun establishing connection to {}", remote_addr);
            return;
        }
        log::warning(logcat, "Failed to begin establishing connection to {}", remote_addr);
    }

    bool LinkManager::have_connection_to(const RouterID& remote) const { return ep->have_conn(remote); }

    bool LinkManager::have_service_connection_to(const RouterID& remote) const { return ep->have_service_conn(remote); }

    bool LinkManager::have_client_connection_to(const RouterID& remote) const { return ep->have_client_conn(remote); }

    void LinkManager::close_all_links()
    {
        log::debug(logcat, "Closing all connections...");

        std::promise<void> p;
        auto f = p.get_future();

        _router.loop()->call([&]() mutable {
            ep->close_all();
            p.set_value();
        });

        f.get();

        ep.reset();
        log::info(logcat, "All connections closed!");
    }

    // TODO: put this in ~LinkManager() after sorting out close sequence and logic
    void LinkManager::stop()
    {
        if (is_stopping)
        {
            return;
        }

        log::info(logcat, "stopping loop");
        is_stopping = true;
        quic_loop->call([this] { ep.reset(); });
        quic_loop.reset();
    }

    void LinkManager::set_conn_persist(const RouterID& remote, std::chrono::milliseconds until)
    {
        if (is_stopping)
            return;

        persisting_conns[remote] = std::max(until, persisting_conns[remote]);

        if (have_client_connection_to(remote))
        {
            // mark this as a client so we don't try to back connect
            clients.Upsert(remote);
        }
    }

    bool LinkManager::is_service_node() const { return _is_service_node; }

    // TODO: this?  perhaps no longer necessary in the same way?
    void LinkManager::check_persisting_conns(std::chrono::milliseconds)
    {
        if (is_stopping)
            return;
    }

    // TODO: this
    nlohmann::json LinkManager::extract_status() const { return {}; }

    void LinkManager::connect_to_keep_alive(int num_conns)
    {
        if (_router.node_db().strict_connect_enabled())
        {
            assert(not _is_service_node);

            // TESTNET: TODO: if given strict-connects, fetch their RCs SPECIFICALLY in bootstrapping
            log::warning(logcat, "FINISH STRICT CONNECT (SEE COMMENT)");
        }

        if (auto rcs = _router.node_db().get_n_random_rcs(
                num_conns, true, [this](const RemoteRC& rc) { return !ep->have_service_conn(rc.router_id()); });
            !rcs.empty())
        {
            for (const auto& rc : rcs)
                connect_to(rc);
        }
        else
            log::warning(logcat, "NodeDB query for {} random RCs for connection returned none", num_conns);
    }

    void LinkManager::regenerate_and_gossip_rc()
    {
        log::info(logcat, "Regenerating and gossiping RC...");
        gossip_rc(_router.local_rid(), _router.update_rc_for_gossiping());
    }

    void LinkManager::gossip_rc(const RouterID& last_sender, const RemoteRC& rc)
    {
        ep->for_each_service_conn(
            [last_sender = last_sender, gossip_src = rc.router_id(), payload = GossipRC::serialize(last_sender, rc)](
                RouterID rid, std::shared_ptr<link::Connection> conn) mutable {
                if (rid == gossip_src or rid == last_sender)
                    return;

                conn->control_stream->command("gossip_rc", payload, [](auto) {});
            });
    }

    void LinkManager::handle_gossip_rc(quic::message m)
    {
        // RemoteRC constructor wraps deserialization in a try/catch
        RemoteRC rc;
        RouterID src;

        try
        {
            oxenc::bt_dict_consumer btdc{m.body()};

            btdc.required("r");
            rc = RemoteRC{btdc.consume_dict_data(), _router.netid()};
            src.from_relay_address(btdc.require<std::string>("s"));
        }
        catch (const std::exception& e)
        {
            log::critical(logcat, "Exception handling GossipRC request: {}", e.what());
            return;
        }

        log::trace(logcat, "Handling GossipRC request (sender:{}, rc:{})...", src, rc);

        if (_router.node_db().verify_store_gossip_rc(rc))
        {
            log::info(logcat, "Received updated RC (rid:{}), forwarding to peers", rc.router_id().short_string());
            gossip_rc(_router.local_rid(), rc);
        }
        else
            log::trace(logcat, "Received known or old RC, not storing or forwarding.");
    }

    // TODO: can probably use ::send_control_message instead. Need to discuss the potential
    // difference in calling Endpoint::get_service_conn vs Endpoint::get_conn
    void LinkManager::fetch_bootstrap_rcs(
        const RemoteRC& source, std::string payload, std::function<void(quic::message)> func)
    {
        func = [this, f = std::move(func)](quic::message m) mutable {
            _router.loop()->call([func = std::move(f), msg = std::move(m)]() mutable { func(std::move(msg)); });
        };

        const auto& rid = source.router_id();

        if (auto conn = ep->get_service_conn(rid); conn)
        {
            conn->control_stream->command("bfetch_rcs", std::move(payload), std::move(func));
            log::debug(logcat, "Dispatched bootstrap fetch request!");
            return;
        }

        _router.loop()->call([this, source, payload, f = std::move(func), rid = rid]() mutable {
            connect_and_send(rid, "bfetch_rcs", std::move(payload), std::move(f));
        });
    }

    void LinkManager::handle_fetch_bootstrap_rcs(quic::message m)
    {
        // this handler should not be registered for clients
        assert(_router.is_service_node());
        log::critical(logcat, "Handling bootstrap fetch request...");

        std::optional<RemoteRC> remote;
        size_t quantity;

        try
        {
            oxenc::bt_dict_consumer btdc{m.body()};
            if (btdc.skip_until("l"))
                remote.emplace(btdc.consume_dict_data(), _router.netid());

            quantity = btdc.require<size_t>("q");
        }
        catch (const std::exception& e)
        {
            log::critical(logcat, "Exception handling bootstrap RC Fetch request (body:{}): {}", m.body(), e.what());
            m.respond(messages::ERROR_RESPONSE, true);
            return;
        }

        if (remote)
        {
            auto& remote_rc = *remote;
            if (_router.node_db().registered_routers().contains(remote_rc.router_id()))
            {
                _router.node_db().put_rc(remote_rc);
                log::debug(
                    logcat,
                    "Bootstrap node confirmed RID:{} is registered; approving fetch request and saving RC!",
                    remote_rc.router_id());
            }
            else
                log::warning(
                    logcat,
                    "Bootstrap node failed to confirm RID:{} is not registered; something is wrong",
                    remote_rc.router_id());
        }

        auto& src = _router.node_db().get_known_rcs();
        auto count = src.size();

        // if quantity is 0, then the service node requesting this wants all the RC's; otherwise,
        // send the amount requested in the message
        quantity = quantity == 0 || quantity > count ? count : quantity;

        auto now = llarp::time_now_ms();

        std::vector<std::string_view> rcs;
        rcs.reserve(quantity);
        for (const auto& [rid, rc] : src)
        {
            if (not rc.is_expired(now))
            {
                rcs.push_back(rc.view());
                if (rcs.size() > quantity)
                    break;
            }
        }
        if (rcs.empty())
        {
            m.respond("No RCs", true);
            return;
        }

        std::ranges::shuffle(rcs, llarp::csrng);
        oxenc::bt_dict_producer btdp;
        {
            auto rc_list = btdp.append_list("r");
            rc_list.reserve(rcs[0].size() * (rcs.size() + 1));  // might be a waste of time
            for (const auto& rc : rcs)
                rc_list.append_encoded(rc);
        }
        m.respond(std::move(btdp).str());
    }

    void LinkManager::fetch_rcs(const RouterID& source, std::string payload, std::function<void(quic::message)> func)
    {
        // this handler should not be registered for service nodes
        assert(not _router.is_service_node());

        send_control_message(source, "fetch_rcs", std::move(payload), std::move(func));
    }

    void LinkManager::_handle_fetch_rcs(quic::message m, std::optional<std::string> inner_body)
    {
        log::debug(logcat, "Handling FetchRC request...");
        // this handler should not be registered for clients
        assert(_router.is_service_node());

        std::set<RouterID> explicit_ids;

        try
        {
            auto btdc = inner_body ? oxenc::bt_dict_consumer{*inner_body} : oxenc::bt_dict_consumer{m.body()};
            for (auto sublist = btdc.require<oxenc::bt_list_consumer>("x"); !sublist.is_finished();)
                explicit_ids.emplace(sublist.consume_span<uint8_t, 32>());
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception handling RC Fetch request: {}", e.what());
            m.respond(messages::ERROR_RESPONSE, true);
            return;
        }

        oxenc::bt_dict_producer btdp;
        {
            auto sublist = btdp.append_list("r");

            int count = 0;
            for (const auto& rid : explicit_ids)
            {
                if (auto* maybe_rc = _router.node_db().get_rc(rid))
                {
                    sublist.append_encoded(maybe_rc->view());
                    ++count;
                }
            }
            log::info(logcat, "Returning {} RCs for FetchRC request...", count);
        }

        m.respond(std::move(btdp).str());
    }

    void LinkManager::fetch_router_ids(const RouterID& via, std::function<void(quic::BTRequestStream&)> send_hook)
    {
        if (auto conn = ep->get_conn(via); conn)
        {
            log::debug(logcat, "Batch dispatching FetchRID requests to {}", via);
            return send_hook(*conn->control_stream);
        }

        log::debug(logcat, "Queueing FetchRID batch send to {}...", via);

        _router.loop()->call([this, rid = via, send_hook = std::move(send_hook)]() mutable {
            connect_and_send(std::move(rid), std::move(send_hook));
        });
    }

    void LinkManager::handle_fetch_router_ids(quic::message m)
    {
        log::trace(logcat, "Handling FetchRIDs request...");
        // this handler should not be registered for clients
        assert(_router.is_service_node());

        RouterID source;
        RouterID local = router().local_rid();

        try
        {
            oxenc::bt_dict_consumer btdc{m.body()};
            source.assign(btdc.require_span<std::byte, RouterID::SIZE>("s"));
        }
        catch (const std::exception& e)
        {
            log::critical(logcat, "Error fulfilling FetchRIDs request: {}; body: {}", e.what(), m.body());
            m.respond(messages::ERROR_RESPONSE, true);
            return;
        }

        if (source != local)
        {
            log::trace(logcat, "Relaying FetchRID request (body: {}) to intended target RID:{}", m.body(), source);

            auto payload = FetchRID::serialize(source);
            send_control_message(
                source, "fetch_rids", std::move(payload), [original = std::move(m)](quic::message msg) mutable {
                    original.respond(msg.body(), msg.is_error());
                });
            return;
        }

        const auto& known_rids = _router.node_db().get_known_rids();
        oxenc::bt_dict_producer btdp;

        {
            auto btlp = btdp.append_list("r");

            for (const auto& rid : known_rids)
                btlp.append(rid.to_view());
        }

        btdp.append_signature("~", [this](std::span<const std::byte> to_sign) {
            std::array<std::byte, SIGSIZE> sig;

            if (!crypto::sign(sig, _router.identity(), to_sign))
                throw std::runtime_error{"Failed to sign fetch RouterIDs response"};

            return sig;
        });

        log::debug(logcat, "Returning ALL ({}) locally held RIDs to FetchRIDs request!", known_rids.size());
        m.respond(std::move(btdp).str());
    }

    void LinkManager::_handle_resolve_sns(
        [[maybe_unused]] quic::message m, [[maybe_unused]] std::optional<std::string> inner_body)
    {
#ifdef LOKINET_EMBEDDED_ONLY
        throw std::logic_error{"This lokinet is not a service node!"};
#else
        log::trace(logcat, "Received request to publish client contact!");

        std::string name_hash;

        try
        {
            if (inner_body)
                name_hash = ResolveSNS::deserialize(oxenc::bt_dict_consumer{*inner_body});
            else
                name_hash = ResolveSNS::deserialize(oxenc::bt_dict_consumer{m.body()});
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception: {}", e.what());
            return m.respond(messages::ERROR_RESPONSE, true);
        }

        _router.rpc_client()->lookup_sns_hash(
            name_hash, [prev_msg = std::move(m)](std::optional<EncryptedSNSRecord> maybe_enc) mutable {
                if (maybe_enc)
                {
                    log::info(logcat, "RPC lookup successfully returned encrypted SNS record!");
                    prev_msg.respond(ResolveSNS::serialize_response(*maybe_enc));
                }
                else
                {
                    log::warning(logcat, "RPC lookup could not find SNS registry!");
                    prev_msg.respond(ResolveSNS::NOT_FOUND, true);
                }
            });
#endif
    }

    void LinkManager::_handle_publish_cc(quic::message m, std::optional<std::string> inner_body)
    {
        log::trace(logcat, "Received request to publish client contact!");

        EncryptedClientContact enc;
        std::optional<RouterID> sender = std::nullopt;

        try
        {
            if (inner_body)
                std::tie(enc, sender) = PublishClientContact::deserialize(oxenc::bt_dict_consumer{*inner_body});
            else
                std::tie(enc, sender) = PublishClientContact::deserialize(oxenc::bt_dict_consumer{m.body()});
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception: {}: payload: {}", e.what(), buffer_printer{m.body()});
            return m.respond(messages::ERROR_RESPONSE, true);
        }

        if (enc.is_expired())
        {
            log::warning(logcat, "Received expired EncryptedClientContact!");
            return m.respond(PublishClientContact::EXPIRED, true);
        }

        if (not enc.verify())
        {
            log::warning(logcat, "Received invalid EncryptedClientContact!");
            return m.respond(PublishClientContact::INVALID, true);
        }

        if (not _is_service_node)
        {
            if (not sender.has_value())
            {
                log::warning(logcat, "Received new EncryptedClientContact from path control with no RouterID!");
                return m.respond(messages::ERROR_RESPONSE, true);
            }

            auto intro = enc.decrypt(*sender);

            // error message prints in ::decrypt(...)
            if (not intro)
                return m.respond(messages::ERROR_RESPONSE, true);

            if (auto session = _router.session_endpoint().get_session(NetworkAddress{*sender, true}))
            {
                log::debug(logcat, "Storing ClientContact for remote rid:{}", *sender);
                _router.contact_db().put_cc(std::move(enc));

                session->update_outbound_remote_intros(std::move(*intro).intros());

                return m.respond(messages::OK_RESPONSE);
            }

            log::warning(logcat, "Could not find session (remote: {}) for updated ClientContact!", *sender);
            return m.respond(messages::ERROR_RESPONSE, true);
        }

        auto dht_key = enc.key();

        // If the optional was nullopt, then this was a relay <-> relay request. As a result, we should NOT
        // allow it to continue propagating
        if (not inner_body)
        {
            log::debug(logcat, "Received relayed PublishClientContact request (key: {}); accepting...", dht_key);
            // TODO FIXME: This is wrong: we should only be storing in our cc *if* we are one of the
            // 4 closest.
            _router.contact_db().put_cc(std::move(enc));
            return m.respond(messages::OK_RESPONSE);
        }

        auto local_rid = _router.local_rid();

        auto closest_rcs = _router.node_db().find_many_closest_to(dht_key, path::DEFAULT_PATHS_HELD);
        if (closest_rcs.empty())
        {
            return m.respond("No RCs available!", true);
        }

        // TODO FIXME: why is there just one closest that we propagate to?  This CC needs to end up
        // at all 4 closest positions, not just the one closest.
        const auto& closest_peer = closest_rcs.front()->router_id();

        for (const auto* rc : closest_rcs)
        {
            auto& rid = rc->router_id();

            log::debug(logcat, "Closest RCs to received ClientContact: {}", rid);

            if (rid == local_rid)
            {
                log::info(
                    logcat,
                    "Received PublishClientContact (key: {}) for which we are a candidate; accepting...",
                    dht_key);
                _router.contact_db().put_cc(std::move(enc));
                return m.respond(messages::OK_RESPONSE);
            }
        }

        // TODO FIXME: these should go to the *four* closest, not the single closest.
        log::info(
            logcat,
            "Received PublishClientContact (key: {}); propagating to closest peer (rid: {})...",
            enc.key(),
            closest_peer);

        send_control_message(
            closest_peer,
            "publish_cc",
            PublishClientContact::serialize(std::move(enc), std::move(sender)),
            [prev_msg = std::move(m)](quic::message msg) mutable {
                log::info(
                    logcat,
                    "Relayed PublishClientContact {}! Relaying response...",
                    msg                 ? "SUCCEEDED"
                        : msg.timed_out ? "timed out"
                                        : "failed");
                log::trace(logcat, "Relayed PublishClientContact response: {}", buffer_printer{msg.body()});
                prev_msg.respond(msg.body(), msg.is_error());
            });
    }

    void LinkManager::_handle_find_cc(quic::message m, std::optional<std::string> inner_body)
    {
        log::trace(logcat, "Received request to find client contact!");

        hash_key dht_key;

        try
        {
            if (inner_body)
                dht_key = FindClientContact::deserialize(oxenc::bt_dict_consumer{*inner_body});
            else
                dht_key = FindClientContact::deserialize(oxenc::bt_dict_consumer{m.body()});
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception: {}", e.what());
            return m.respond(messages::ERROR_RESPONSE, true);
        }

        if (auto maybe_cc = _router.contact_db().get_encrypted_cc(dht_key))
        {
            log::info(
                logcat,
                "Received FindClientContact request (key: {}); returning local EncryptedClientContact...",
                dht_key);
            return m.respond(FindClientContact::serialize_response(*maybe_cc));
        }

        // If the optional was nullopt, then this was a relay <-> relay request. As a result, we should NOT
        // allow it to continue propagating
        if (not inner_body)
        {
            log::critical(
                logcat,
                "Received relayed FindClientContact request (key: {}); could not find locally, relaying "
                "error...",
                dht_key);
            return m.respond(FindClientContact::NOT_FOUND, true);
        }

        auto local_rid = _router.local_rid();

        auto closest_rcs = _router.node_db().find_many_closest_to(dht_key, path::DEFAULT_PATHS_HELD);
        if (closest_rcs.empty())
            return m.respond("No RCs!", true);

        auto n_closest = closest_rcs.size();

        for (const auto* rc : closest_rcs)
        {
            auto& rid = rc->router_id();

            if (rid == local_rid)
            {
                log::warning(
                    logcat,
                    "We are closest peer for FindClientContact request (key: {}); no EncryptedClientContact "
                    "found locally!",
                    dht_key);
                return m.respond(FindClientContact::NOT_FOUND, true);
            }
        }

        auto counter = std::make_shared<size_t>(n_closest);

        auto hook = [prev_msg = std::move(m), counter](quic::message msg) mutable {
            if (*counter == 0)
                return;

            if (msg)
            {
                *counter = 0;
                log::info(logcat, "Relayed FindClientContact request SUCCEEDED! Relaying response...");
                log::trace(logcat, "Relayed FindClientContact response: {}", buffer_printer{msg.body()});
            }
            else if (--*counter == 0)
            {
                log::warning(logcat, "All FindClientContact requests FAILED! Relaying response...");
            }
            else
                return;

            prev_msg.respond(msg.body(), msg.is_error());
        };

        log::info(logcat, "Relaying FindClientContactMessage (key: {}) to {} peers", dht_key, n_closest);

        for (const auto& rc : closest_rcs)
        {
            send_control_message(rc->router_id(), "find_cc", FindClientContact::serialize(dht_key), hook);
        }
    }

    void LinkManager::handle_path_build(quic::message m, const RouterID& from)
    {
        if (!_router.path_context.is_transit_allowed())
        {
            log::warning(logcat, "got path build request when not permitting transit");
            return m.respond(PATH::BUILD::NO_TRANSIT, true);
        }

        try
        {
            auto frames = ONION::deserialize_frames(m.body());
            auto n_frames = frames.size();

            if (n_frames != path::MAX_LEN)
            {
                log::info(logcat, "Path build message with wrong number of frames: {}", frames.size());
                return m.respond(PATH::BUILD::BAD_FRAMES, true);
            }

            log::trace(logcat, "Deserializing frame: {}", buffer_printer{frames.front()});

            auto hop = PATH::BUILD::deserialize_hop(oxenc::bt_dict_consumer{frames.front()}, _router, from);

            // we are terminal hop and everything is okay
            if (hop->upstream() == _router.local_rid())
            {
                log::info(logcat, "We are the terminal hop; path build succeeded");
                if (not hop->terminal_hop)
                {
                    // TESTNET: remove this eventually
                    log::critical(
                        logcat, "DANIEL FIX THIS: Hop is terminal hop; constructor should have flipped this boolean");
                    hop->terminal_hop = true;
                }

                _router.path_context.put_transit_hop(std::move(hop));
                return m.respond(messages::OK_RESPONSE);
            }

            // rotate our frame to the back
            std::ranges::rotate(frames, frames.begin() + 1);

            // clear our frame, to be randomized after onion step and appended
            frames.back().clear();

            auto onion_nonce = hop->kx.nonce ^ hop->kx.xor_nonce;

            // (de-)onion each further frame using the established shared secret and
            // onion_nonce = nonce ^ nonceXOR
            // Note: final value passed to crypto::onion is xor factor, but that's for *after* the
            // onion round to compute the return value, so we don't care about it.
            // for (auto& element : frames)
            for (size_t i = 0; i < n_frames - 1; ++i)
            {
                crypto::onion(as_bspan(frames[i]), hop->kx.shared_secret, onion_nonce, onion_nonce);
            }

            // randomize final frame
            randombytes_buf(reinterpret_cast<unsigned char*>(frames.back().data()), frames.back().size());

            auto upstream = hop->upstream();

            send_control_message(
                std::move(upstream),
                "path_build",
                ONION::serialize_frames(std::move(frames)),
                [this, transit_hop = std::move(hop), prev_message = std::move(m)](quic::message m) mutable {
                    if (m)
                    {
                        log::info(
                            logcat,
                            "Upstream returned successful path build response; locally storing Hop ({}) and "
                            "relaying",
                            transit_hop->to_string());
                        _router.path_context.put_transit_hop(std::move(transit_hop));
                        return prev_message.respond(messages::OK_RESPONSE, false);
                    }

                    log::info(
                        logcat,
                        "Upstream ({}) returned path build {}; relaying...",
                        transit_hop->upstream(),
                        m.timed_out ? "time out" : "failure");

                    return prev_message.respond(m.body(), m.is_error());
                });
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception: {}: input: {}", e.what(), m.body());
            // We can respond with the exception string, as all exceptions thrown in the parsing functions
            // (ex: `TransitHop::deserialize_hop(...)`) contain the correct response bodies
            return m.respond(e.what(), true);
        }
    }

    void LinkManager::_handle_path_control(quic::message m, std::optional<std::string> inner_body)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        HopID hop_id;
        std::string payload;
        SymmNonce nonce;

        try
        {
            if (inner_body)
                std::tie(hop_id, nonce, payload) = ONION::deserialize_hop(oxenc::bt_dict_consumer{*inner_body});
            else
                std::tie(hop_id, nonce, payload) = ONION::deserialize_hop(oxenc::bt_dict_consumer{m.body()});
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception: {}", e.what());
            log::warning(logcat, "Payload: {}", inner_body ? buffer_printer{*inner_body} : buffer_printer{m.body()});
            return m.respond(messages::ERROR_RESPONSE, true);
        }

        if (not _is_service_node)
        {
            auto path = _router.path_context.get_path(hop_id);

            if (not path)
            {
                log::warning(logcat, "Client received path control with unknown rxID: {}", hop_id);
                return m.respond(messages::ERROR_RESPONSE, true);
            }

            log::trace(logcat, "Received path control for local client: {}", buffer_printer{payload});

            for (auto& hop : path->hops)
            {
                nonce = crypto::onion(as_bspan(payload), hop.kx.shared_secret, nonce, hop.kx.xor_nonce);

                log::trace(logcat, "xchacha20 -> {}", buffer_printer{payload});
            }

            return handle_path_request(std::move(m), as_bspan(payload));
        }

        auto hop = _router.path_context.get_transit_hop(hop_id);

        if (not hop)
        {
            log::warning(logcat, "Received path control with unknown next hop (ID: {})", hop_id);
            return m.respond(messages::ERROR_RESPONSE, true);
        }

        auto onion_nonce = nonce ^ hop->kx.xor_nonce;

        crypto::onion(as_bspan(payload), hop->kx.shared_secret, onion_nonce, hop->kx.xor_nonce);

        if (not inner_body)
        {
            // if terminal hop, payload should contain a request (e.g. "sns_resolve"); handle and respond.
            if (hop->terminal_hop)
            {
                log::debug(logcat, "We are terminal hop for path request: {}", hop->to_string());
                return handle_path_request(std::move(m), as_bspan(payload));
            }

            log::debug(logcat, "We are intermediate hop for path request: {}", hop->to_string());
        }
        else
        {
            log::info(
                logcat, "We are bridge node for aligned path request ({})! Forwarding downstream", hop->to_string());
            log::trace(logcat, "Payload: {}", buffer_printer{*inner_body});
        }

        auto next_ids = hop->next_id(hop_id);

        if (not next_ids)
        {
            log::error(logcat, "Failed to query hop ({}) for next ids (input: {})", hop->to_string(), hop_id);
            return m.respond(messages::ERROR_RESPONSE, true);
        }

        std::string new_payload = ONION::serialize_hop(next_ids->second, onion_nonce, as_bspan(payload));

        send_control_message(
            next_ids->first,
            "path_control",
            std::move(new_payload),
            [hop_weak = std::weak_ptr{hop}, hop_id, prev_message = std::move(m)](quic::message response) mutable {
                auto hop = hop_weak.lock();
                if (not hop)
                {
                    log::warning(logcat, "Received response to path control message with non-existent TransitHop!");
                    return prev_message.respond(messages::ERROR_RESPONSE, true);
                }

                if (response)
                    log::debug(logcat, "Path control message returned successfully!");
                else if (response.timed_out)
                    log::warning(logcat, "Path control message returned as time out!");
                else
                    log::warning(logcat, "Path control message returned as error!");

                prev_message.respond(response.body(), response.is_error());

                // TODO: onion encrypt path message responses
                // HopID hop_id;
                // SymmNonce nonce;
                // std::string payload;

                // try
                // {
                //     std::tie(hop_id, nonce, payload) =
                //     ONION::deserialize_hop(oxenc::bt_dict_consumer{response.body()});
                // }
                // catch (const std::exception& e)
                // {
                //     log::warning(logcat, "Exception: {}; payload: {}", e.what(),
                //     buffer_printer{response.body()}); return prev_message.respond(messages::ERROR_RESPONSE,
                //     true);
                // }

                // auto resp_payload = ONION::serialize_hop(hop_id.to_view(), nonce, std::move(payload));
                // prev_message.respond(std::move(resp_payload), false);
            });
    }

    void LinkManager::handle_path_control(quic::message m) { return _handle_path_control(std::move(m)); }

    void LinkManager::handle_path_data_message(quic::datagram dgram)
    {
        // not a registered handler, use loop-call
        auto data = std::move(dgram).extract();
        _router.loop()->call([this, message = std::move(data)]() mutable {
            HopID hop_id;
            std::string payload;
            SymmNonce nonce;

            try
            {
                std::tie(hop_id, nonce, payload) =
                    ONION::deserialize_hop(oxenc::bt_dict_consumer{std::span<const std::byte>{message}});
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Exception: {}", e.what());
                return;
            }

            if (not _is_service_node)
            {
                auto path = _router.path_context.get_path(hop_id);

                if (not path)
                {
                    log::warning(logcat, "Client received path data with unknown rxID: {}", hop_id);
                    return;
                }

                for (auto& hop : path->hops)
                {
                    nonce = crypto::onion(as_bspan(payload), hop.kx.shared_secret, nonce, hop.kx.xor_nonce);

                    log::trace(logcat, "xchacha20 -> {}", buffer_printer{payload});
                }

                return handle_path_session_data(as_bspan(payload));
            }

            auto hop = _router.path_context.get_transit_hop(hop_id);

            if (not hop)
            {
                log::warning(logcat, "Received path data with unknown next hop (ID: {})", hop_id);
                return;
            }

            auto onion_nonce = nonce ^ hop->kx.xor_nonce;

            crypto::onion(as_bspan(payload), hop->kx.shared_secret, onion_nonce, hop->kx.xor_nonce);

            std::optional<std::pair<RouterID, HopID>> next_ids = std::nullopt;
            std::string next_payload;

            log::trace(logcat, "We are {} hop for path data", hop->terminal_hop ? "terminal" : "intermediate");

            log::trace(logcat, "Path data: {}", buffer_printer{payload});

            // if terminal hop, pass to the correct path expecting to receive this message
            if (hop->terminal_hop)
            {
                HopID ihid;
                std::string intermediate;

                try
                {
                    std::tie(ihid, intermediate) =
                        PATH::DATA::deserialize_intermediate(oxenc::bt_dict_consumer{payload});
                }
                catch (const std::exception& e)
                {
                    log::warning(
                        logcat, "Path data intermediate payload exception: {}: {}", e.what(), buffer_printer{payload});
                    return;
                }

                log::trace(logcat, "Inbound path rxid:{}, outbound path txid:{}", hop_id, ihid);

                auto next_hop = _router.path_context.get_transit_hop(ihid);

                if (not next_hop)
                {
                    log::warning(logcat, "We are bridge node for path data message with unknown txID: {}", ihid);
                    return;
                }

                next_ids = next_hop->next_id(ihid);

                if (hop_id == next_ids->second)
                {
                    log::trace(logcat, "Received path data for local relay: {}", buffer_printer{intermediate});
                    return handle_path_session_data(as_bspan(intermediate));
                }

                log::debug(logcat, "Bridging path data message to hop: {}", next_hop->to_string());
                onion_nonce ^= next_hop->kx.xor_nonce;

                crypto::onion(as_bspan(intermediate), next_hop->kx.shared_secret, onion_nonce, next_hop->kx.xor_nonce);

                if (not next_ids)
                {
                    log::error(
                        logcat, "Failed to query hop ({}) for next ids (input: {})", next_hop->to_string(), hop_id);
                    return;
                }

                next_payload = ONION::serialize_hop(next_ids->second, onion_nonce, as_bspan(intermediate));
            }
            else
            {
                next_ids = hop->next_id(hop_id);

                if (not next_ids)
                {
                    log::error(logcat, "Failed to query hop ({}) for next ids (input: {})", hop->to_string(), hop_id);
                    return;
                }

                next_payload = ONION::serialize_hop(next_ids->second, onion_nonce, as_bspan(payload));
            }

            send_data_message(next_ids->first, std::move(next_payload));
        });
    }

    void LinkManager::handle_path_session_data(std::span<std::byte> payload)
    {
        try
        {
            auto [tag, data] = PATH::DATA::deserialize_inner(payload);

            if (auto session = _router.session_endpoint().get_session(tag))
            {
                session->recv_path_data_message(data);
            }
            else
            {
                log::warning(logcat, "Could not find session (tag:{}) to relay path data message!", tag);
            }
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception: {}: {}", e.what(), buffer_printer{payload});
        }
    }

    void LinkManager::handle_path_request(quic::message m, std::span<const std::byte> payload)
    {
        std::string endpoint, body;

        try
        {
            std::tie(endpoint, body) = PATH::CONTROL::deserialize(oxenc::bt_dict_consumer{payload});

            if (_is_service_node and endpoint == "path_control")
            {
                log::info(logcat, "Received path control relay request; deserializing intermediate payload...");
                auto [_, i_body] = PATH::CONTROL::deserialize(oxenc::bt_dict_consumer{std::move(body)});
                return _handle_path_control(std::move(m), std::move(i_body));
            }
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception: {}; Payload: {}", e.what(), buffer_printer{payload});
            return m.respond(messages::serialize_status_response("ERROR"), true);
        }

        if (auto it = path_requests.find(endpoint); it != path_requests.end())
        {
            log::debug(logcat, "Received path control request (`{}`); invoking endpoint...", endpoint);
            std::invoke(it->second, this, std::move(m), std::move(body));
        }
        else
            log::warning(logcat, "Received path control request (`{}`), which has no local handler!", endpoint);
    }

    void LinkManager::_handle_initiate_session(quic::message m, std::optional<std::string> inner_body)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        NetworkAddress initiator;
        HopID remote_pivot_txid;
        HopID local_pivot_txid;
        bool use_tun{};
        shared_kx_data kx_data;
        std::optional<std::string> maybe_auth = std::nullopt;

        try
        {
            if (inner_body)
            {
                if (_is_service_node)
                    std::tie(initiator, local_pivot_txid, remote_pivot_txid, use_tun, maybe_auth) =
                        InitiateSession::deserialize(oxenc::bt_dict_consumer{*inner_body});
                else
                    std::tie(kx_data, initiator, local_pivot_txid, remote_pivot_txid, use_tun, maybe_auth) =
                        InitiateSession::decrypt_deserialize(oxenc::bt_dict_consumer{*inner_body}, _router.identity());
            }
            else  // TESTNET: this route is superfluous for this type of request almost surely, revisit soon
                std::tie(kx_data, initiator, local_pivot_txid, remote_pivot_txid, use_tun, maybe_auth) =
                    InitiateSession::decrypt_deserialize(oxenc::bt_dict_consumer{m.body()}, _router.identity());
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception: {}", e.what());
        }

        if (initiator.router_id() == _router.local_rid())
        {
            log::warning(logcat, "Received request to initiate session from local instance; ignoring!");
            return m.respond(InitiateSession::BAD_ADDRESS, true);
        }

        if (maybe_auth and not _router.session_endpoint().validate(initiator, maybe_auth))
        {
            log::warning(logcat, "Failed to authenticate session initiation request from remote:{}", initiator);
            return m.respond(InitiateSession::AUTH_ERROR, true);
        }

        std::shared_ptr<session_path_interface> pi = nullptr;

        if (not _is_service_node)
        {
            pi = _router.path_context.get_path(local_pivot_txid);

            if (not pi)
            {
                log::warning(
                    logcat, "Failed to find local path for new inbound session over pivot txid: {}", local_pivot_txid);
                return m.respond(InitiateSession::BAD_ROUTE, true);
            }
        }
        else
        {
            if (local_pivot_txid != remote_pivot_txid)
            {
                log::warning(logcat, "Received misrouted path-request to initiate client<->client session...");
                return m.respond(InitiateSession::BAD_ROUTE, true);
            }

            auto hop = _router.path_context.get_transit_hop(local_pivot_txid);

            if (not hop)
            {
                log::warning(
                    logcat, "Received path-request to initiate session with unknown hop (ID: {})", local_pivot_txid);
                return m.respond(InitiateSession::BAD_ROUTE, true);
            }

            if (not hop->terminal_hop)
            {
                log::warning(
                    logcat,
                    "Received path-request to initiate session and we are NOT terminal hop (ID: {})",
                    local_pivot_txid);
                return m.respond(InitiateSession::BAD_ROUTE, true);
            }

            pi = std::make_shared<path::SessionHop>(*hop, _router.session_endpoint());
            kx_data = hop->kx;
        }

        assert(pi);

        if (auto tag = _router.session_endpoint().prefigure_session(
                std::move(initiator), std::move(remote_pivot_txid), std::move(pi), std::move(kx_data), use_tun))
        {
            log::debug(logcat, "InboundSession (tag:{}) configured successfully!", *tag);
            return m.respond(InitiateSession::serialize_response(*tag));
        }

        log::warning(logcat, "Failed to configure InboundSession!");

        m.respond(messages::ERROR_RESPONSE, true);
    }

    void LinkManager::handle_initiate_session(quic::message m) { return _handle_initiate_session(std::move(m)); }

    void LinkManager::_handle_path_switch(quic::message m, std::optional<std::string> inner_body)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        session_tag tag;
        HopID remote_pivot_txid, local_pivot_txid;

        std::string_view body{inner_body ? *inner_body : m.body()};
        try
        {
            std::tie(tag, remote_pivot_txid, local_pivot_txid) =
                SessionPathSwitch::deserialize(oxenc::bt_dict_consumer{body});
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception: {}", e.what());
            return m.respond(messages::ERROR_RESPONSE, true);
        }

        if (!_is_service_node)
        {
            auto path = _router.path_context.get_path(local_pivot_txid);

            if (not path)
            {
                log::warning(
                    logcat, "Received path-switch request for unknown local path (pivot txid:{})", local_pivot_txid);
                return m.respond(SessionPathSwitch::BAD_ID, true);
            }

            if (_router.session_endpoint().recv_path_switch(tag, std::move(remote_pivot_txid), std::move(path)))
                return m.respond(messages::OK_RESPONSE);
        }
        else
        {
            auto hop = _router.path_context.get_transit_hop(local_pivot_txid);

            if (not hop)
            {
                log::warning(
                    logcat, "Received path-switch request for unknown local hop (pivot txid:{})", local_pivot_txid);
                return m.respond(SessionPathSwitch::BAD_ID, true);
            }

            if (_router.session_endpoint().recv_path_switch(
                    tag,
                    std::move(remote_pivot_txid),
                    std::make_shared<path::SessionHop>(*hop, _router.session_endpoint())))
                return m.respond(messages::OK_RESPONSE);
        }

        log::warning(logcat, "Received path-switch request for unknown session (tag:{})", tag);
        return m.respond(SessionPathSwitch::BAD_TAG, true);
    }

    void LinkManager::handle_path_switch(quic::message m) { return _handle_path_switch(std::move(m)); }

    void LinkManager::_handle_path_ping(quic::message m, std::optional<std::string>)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        m.respond(messages::OK_RESPONSE);
    }

    void LinkManager::_handle_close_session(quic::message m, std::optional<std::string> inner_body)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        session_tag tag;

        try
        {
            if (inner_body)
                tag = CloseSession::deserialize(oxenc::bt_dict_consumer{*inner_body});
            else
                tag = CloseSession::deserialize(oxenc::bt_dict_consumer{m.body()});

            if (_router.session_endpoint().close_session(tag))
                return m.respond(messages::OK_RESPONSE);
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception: {}", e.what());
        }

        m.respond(messages::ERROR_RESPONSE, true);
    }

    void LinkManager::handle_close_session(quic::message m) { return _handle_close_session(std::move(m)); }

    void LinkManager::handle_path_latency(quic::message m)
    {
        try
        {
            oxenc::bt_dict_consumer btdc{m.body()};
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception: {}", e.what());
            return m.respond(messages::ERROR_RESPONSE, true);
        }
    }

    void LinkManager::handle_path_latency_response(quic::message m)
    {
        try
        {
            oxenc::bt_dict_consumer btdc{m.body()};
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception: {}", e.what());
            return;
        }
    }

}  // namespace llarp
