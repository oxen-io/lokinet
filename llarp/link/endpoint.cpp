#include "endpoint.hpp"

#include "link_manager.hpp"

#include <llarp/nodedb.hpp>
#include <llarp/util/time.hpp>

// TODO FIXME: This include shouldn't be necessary to actually construct an Endpoint, but apparently
// is:
#include <oxen/quic/context.hpp>
#include <sodium/crypto_generichash_blake2b.h>

namespace llarp::link
{
    static auto logcat = log::Cat("link.endpoint");

    void relay_conn::set_conn(std::shared_ptr<link::Connection> c, bool is_inbound)
    {
        auto& ptr = is_inbound ? inbound : outbound;
        if (ptr)
            ptr->close_quietly();
        ptr = std::move(c);

        if (is_inbound ? not outbound or inbound_wins : not inbound or not inbound_wins)
            conn = ptr.get();
    }

    void relay_conn::close_quietly(bool direction_inbound)
    {
        auto& to_close = direction_inbound ? inbound : outbound;
        if (not to_close)
            return;

        to_close->close_quietly();
        to_close.reset();

        // Switch preferred conn to the other direction (will be nullptr if the other direction
        // isn't established):
        conn = (direction_inbound ? outbound : inbound).get();
    }

    void relay_conn::close_all_quietly()
    {
        if (inbound)
            inbound->close_quietly();
        if (outbound)
            outbound->close_quietly();
        inbound = outbound = nullptr;
        conn = nullptr;
    }

    void relay_conn::close_redundant() { close_quietly(not inbound_wins); }

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

    Endpoint::Endpoint(Manager& lm)
        : manager{lm},
          router{lm.router},
          loop{std::make_unique<quic::Loop>()},
          tls_creds{quic::GNUTLSCreds::make_from_ed_keys(
              {reinterpret_cast<const char*>(router.identity().data()), 32},
              {reinterpret_cast<const char*>(router.local_rid().data()), 32})}
    {
        std::optional<quic::opt::inbound_alpns> inbound_alpn;
        if (router.is_service_node)
            inbound_alpn.emplace({RELAY_ALPN, CLIENT_ALPN});

        endpoint = quic::Endpoint::endpoint(
            *loop,
            router.listen_addr(),
            quic::opt::static_secret{make_static_secret(router.identity())},
            [this](quic::Connection& conn) { on_conn_established(conn); },
            [this](quic::Connection& conn, uint64_t ec) { on_conn_closed(conn, ec); },
            [this](quic::datagram dgram) {
                // Transfer handling to the router loop:
                router.loop.call([this, msg = std::move(dgram).extract()]() mutable {
                    manager.handle_path_data_message(std::move(msg));
                });
            },
            inbound_alpn,
            quic::opt::outbound_alpns{{router.is_service_node ? RELAY_ALPN : CLIENT_ALPN}},
            quic::opt::enable_datagrams{quic::Splitting::ACTIVE});

        if (router.is_service_node)
        {
            tls_creds->set_key_verify_callback([this](const std::span<const uint8_t> key, const std::string_view alpn) {
                // NB: this code *must not* call_get into the router event loop, because there are
                // lots of places that router call-get's into endloop.loop, and so any attempt to
                // call-get the other direction is a recipe for deadlock.
                if (key.size() != RouterID::SIZE)
                {
                    log::warning(
                        logcat,
                        "Rejecting incoming connection with invalid/unsupported pubkey ({} bytes, expected {})",
                        key.size(),
                        RouterID::SIZE);
                    return false;
                }
                RouterID other{key.first<32>()};

                if (alpn == CLIENT_ALPN)
                    return true;

                if (other == router.local_rid())
                {
                    log::error(
                        logcat,
                        "Rejecting incoming relay connection from relay with our own key ({})",
                        other.to_network_address());
                    return false;
                }

                if (router.node_db().is_registered(other))
                    return true;

                log::warning(logcat, "Rejecting incoming relay connection from unregistered RID {}", other);
                return false;
            });

            endpoint->listen(
                tls_creds,
                quic::opt::idle_timeout{router.is_service_node ? RELAY_INBOUND_IDLE_TIMEOUT : CLIENT_IDLE_TIMEOUT});
        }
    }

    void Endpoint::start_tickers()
    {
        if (router.is_service_node)
            redundancy_ticker = router.loop.call_every(REDUNDANT_LINGER, [this] { close_redundant(); });
    }

    link::Connection* Endpoint::get_relay_conn(const RouterID& relay) const
    {
        return router.loop.call_get([this, relay]() -> link::Connection* {
            if (router.is_service_node)
            {
                if (auto it = relay_conns.find(relay); it != relay_conns.end())
                    return it->second.conn;
            }
            else if (auto it = client_conns.find(relay); it != client_conns.end())
                return it->second.get();

            return nullptr;
        });
    }

    void Endpoint::close_redundant(std::chrono::milliseconds now)
    {
        for (auto it = relay_bidir.begin(); it != relay_bidir.end();)
        {
            auto& [rid, since] = *it;
            if (now >= since + REDUNDANT_LINGER)
            {
                auto rcit = relay_conns.find(rid);
                // If this assertion fails then something else in here isn't cleaning up properly:
                assert(rcit != relay_conns.end() and rcit->second.inbound and rcit->second.outbound);
                rcit->second.close_redundant();
                it = relay_bidir.erase(it);
            }
            else
                ++it;
        }
    }

    link::Connection* Endpoint::get_client_conn(const RouterID& remote) const
    {
        return router.loop.call_get([this, remote]() -> link::Connection* {
            if (auto itr = client_conns.find(remote); itr != client_conns.end())
                return itr->second.get();
            return nullptr;
        });
    }

    void Endpoint::for_each_relay_conn(std::function<void(const RouterID&, link::Connection&)> func) const
    {
        assert(router.loop.inside());

        if (manager.is_stopping)
            return;

        for (const auto& [rid, relay] : relay_conns)
        {
            assert(relay.conn);
            func(rid, *relay.conn);
        }
    }

    std::unordered_set<RouterID> Endpoint::get_current_relays(bool include_pending) const
    {
        std::unordered_set<RouterID> ret;

        if (router.is_service_node)
        {
            for (auto& [rid, conn] : relay_conns)
            {
                assert(conn.conn);  // nullptr means something left a bad conn in here instead of clearing it out
                ret.insert(rid);
            }
        }
        else
        {
            for (auto& [rid, conn] : client_conns)
            {
                assert(conn);
                ret.insert(rid);
            }
        }
        if (include_pending)
        {
            for (auto& [rid, c] : pending_outbound)
            {
                assert(c);
                ret.insert(rid);
            }
        }

        return ret;
    }

    bool Endpoint::connected_to_relay(const RouterID& relay, bool include_pending) const
    {
        if (router.is_service_node ? relay_conns.contains(relay) : client_conns.contains(relay))
            return true;
        if (include_pending and pending_outbound.contains(relay))
            return true;
        return false;
    }

    void Endpoint::shutdown()
    {
        log::debug(logcat, "Closing all connections");
        for (auto& [rid, conn] : relay_conns)
            conn.close_all_quietly();
        relay_conns.clear();
        relay_bidir.clear();

        for (auto& conn : client_conns)
            conn.second->close_quietly();

        client_conns.clear();

        log::debug(logcat, "Closing quic endpoint");
        endpoint.reset();

        log::info(logcat, "Stopping network endpoint event loop");
        loop.reset();
    }

    std::array<int, 5> Endpoint::relay_connection_counts() const
    {
        if (not router.is_service_node)
            return {0};
        return router.loop.call_get([this] {
            std::array<int, 5> result{0};
            auto& [relays, out, in, pending, clients] = result;

            relays = static_cast<int>(relay_conns.size());
            clients = static_cast<int>(client_conns.size());
            pending = static_cast<int>(pending_outbound.size());
            for (const auto& c : std::views::values(relay_conns))
            {
                if (c.inbound)
                    ++in;
                if (c.outbound)
                    ++out;
            }

            return result;
        });
    }

    std::array<int, 2> Endpoint::client_connection_counts() const
    {
        return router.loop.call_get([this] {
            return std::array{static_cast<int>(client_conns.size()), static_cast<int>(pending_outbound.size())};
        });
    }

    int Endpoint::num_relay_conns(bool include_pending) const
    {
        return router.loop.call_get([this, &include_pending] {
            int c;
            if (router.is_service_node)
            {
                c = static_cast<int>(relay_conns.size());
                if (include_pending)
                    // Check each pending conn instead of just adding pending_outbound.count() because
                    // we don't want to double count a relay if we have a pending outbound connection
                    // to a relay with which we have already established an inbound connection:
                    for (auto& [rid, conn] : pending_outbound)
                        if (!relay_conns.contains(rid))
                            c++;
            }
            else
            {
                // We are a client, and so *all* connections are outbound to a relay that we
                // initiated: Unlike the above, we would never initiate to an already pending or
                // already connected node, so don't have to worry about duplicates.
                c = static_cast<int>(client_conns.size() + pending_outbound.size());
            }
            return c;
        });
    }

    std::pair<bool, quic::BTRequestStream*> Endpoint::ctrl_stream_impl(const RemoteRC& rc)
    {
        assert(router.loop.inside());
        std::pair<bool, quic::BTRequestStream*> result;
        auto& [res_est, res_str] = result;

        auto rid = rc.router_id();

        if (auto* c = get_relay_conn(rid))
        {
            res_est = true;
            res_str = c->control_stream.get();
            return result;
        }

        res_est = false;
        auto& pending = pending_outbound[rid];
        if (!pending)
        {
            log::debug(logcat, "Initiating new connection to send to {}", rid.short_string());
            auto conn = endpoint->connect(
                quic::RemoteAddress{rid.to_view(), rc.addr()},
                tls_creds,
                quic::opt::keep_alive{router.is_service_node ? RELAY_OUTBOUND_KEEP_ALIVE : CLIENT_KEEP_ALIVE},
                quic::opt::idle_timeout{router.is_service_node ? RELAY_OUTBOUND_IDLE_TIMEOUT : CLIENT_IDLE_TIMEOUT},
                [this](quic::Connection& conn) { on_conn_established(conn); });

            auto control_stream = make_control(*conn, rid);

            pending = std::make_shared<Connection>(std::move(conn), std::move(control_stream));
        }
        res_str = pending->control_stream.get();

        return result;
    }

    bool Endpoint::ensure_connection(const RemoteRC& rc)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        return ctrl_stream_impl(rc).first;
    }

    quic::BTRequestStream& Endpoint::control_stream_for(const RemoteRC& rc)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        return *ctrl_stream_impl(rc).second;
    }

    void Endpoint::send_command(
        const RemoteRC& rc,
        std::string endpoint,
        std::vector<std::byte> body,
        std::function<void(quic::message)> response_handler)
    {
        if (manager.is_stopping)
            return;

        if (response_handler)
            // Wrap the handler to transfer to the router loop for execution:
            response_handler = [f = std::move(response_handler), &rloop = router.loop](quic::message m) mutable {
                rloop.call([f = std::move(f), m = std::move(m)]() mutable { f(std::move(m)); });
            };

        control_stream_for(rc).command(std::move(endpoint), std::move(body), std::move(response_handler));
    }

    bool Endpoint::send_command(
        const RouterID& rid,
        std::string endpoint,
        std::vector<std::byte> body,
        std::function<void(quic::message)> response_handler)
    {
        if (manager.is_stopping)
            return false;

        auto* rc = router.node_db().get_rc(rid);
        if (!rc)
        {
            log::error(
                logcat, "Unable to send {} control command to {}: RC not found", endpoint, rid.to_network_address());
            return false;
        }

        send_command(*rc, std::move(endpoint), std::move(body), std::move(response_handler));
        return true;
    }

    bool Endpoint::send_datagram(const RouterID& remote, std::vector<std::byte> body)
    {
        if (manager.is_stopping)
            return false;

        auto* conn = get_relay_conn(remote);
        if (!conn)
            conn = get_client_conn(remote);
        if (!conn)
            // Unlike send_command, if we don't have an established connection yet then we simply
            // drop this rather than trying to store it in a queue.
            return false;

        conn->datagrams->send(std::move(body));
        return true;
    }

    std::shared_ptr<quic::BTRequestStream> Endpoint::make_control(quic::Connection& conn, const RouterID& remote)
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

        manager.register_commands(*control_stream, remote, not router.is_service_node);
        return control_stream;
    }

    void Endpoint::on_inbound_conn(std::shared_ptr<quic::Connection> qconn)
    {
        assert(router.is_service_node);
        assert(qconn->remote_key().size() == RouterID::SIZE);  // Should have been checked in the key verify callback
        RouterID rid{qconn->remote_key().first<RouterID::SIZE>()};

        auto control = make_control(*qconn, rid);
        bool is_relay = qconn->selected_alpn() != CLIENT_ALPN;

        auto conn = std::make_shared<link::Connection>(std::move(qconn), std::move(control));

        if (is_relay)
        {
            auto [it, ins] = relay_conns.emplace(rid, rid < router.local_rid());
            auto& relcon = it->second;
            assert(ins ? !relcon.conn : !!relcon.conn);
            bool already_had_inbound{relcon.inbound};
            relcon.set_conn(std::move(conn), true);
            if (relcon.outbound)
                relay_bidir[rid] = llarp::time_now_ms();

            log::debug(
                logcat,
                "{} incoming relay connection from {} ({} outbound connection)",
                already_had_inbound ? "Replaced existing" : "New",
                rid.to_network_address(true),
                !relcon.outbound          ? "no current"
                    : relcon.inbound_wins ? "have redundant"
                                          : "have more-preferred");
        }
        else
        {
            // We are a service node; this container holds client connections to us:
            auto& cc = client_conns[rid];
            bool replaced{cc};
            if (replaced)
                cc->close_quietly();
            cc = std::move(conn);
            log::debug(
                logcat,
                "{} incoming client connection from {}",
                replaced ? "Replaced existing" : "New",
                rid.to_network_address(false));
        }
    }

    void Endpoint::on_outbound_conn(std::shared_ptr<quic::Connection> qconn)
    {
        assert(qconn->remote_key().size() == RouterID::SIZE);
        RouterID rid{qconn->remote_key().first<RouterID::SIZE>()};

        assert(router.is_service_node == (qconn->selected_alpn() != CLIENT_ALPN));

        auto pit = pending_outbound.find(rid);
        if (pit == pending_outbound.end())
        {
            log::error(logcat, "Internal error: outbound connection completed without a pending connection");
            return;
        }

        if (router.is_service_node)
        {
            auto [it, ins] = relay_conns.emplace(rid, rid < router.local_rid());
            auto& relcon = it->second;
            assert(ins ? !relcon.conn : !!relcon.conn);
            bool already_had_outbound{relcon.outbound};
            if (already_had_outbound)
                log::error(
                    logcat, "Internal error: duplicate outbound connection established, but that shouldn't happen");

            relcon.set_conn(std::move(pit->second), false);
            if (relcon.inbound)
                relay_bidir[rid] = llarp::time_now_ms();

            log::debug(
                logcat,
                "{} outbound connection to {} ({} inbound connection)",
                already_had_outbound ? "Replaced existing" : "Established",
                rid.to_network_address(true),
                !relcon.inbound           ? "no current"
                    : relcon.inbound_wins ? "have more-preferred"
                                          : "have redundant");
        }
        else
        {
            // we are the client; this container holds our client connections (to relays)
            auto& cc = client_conns[rid];
            bool replaced{cc};
            if (replaced)
            {
                log::error(
                    logcat, "Internal error: duplicate outbound connection established, but that shouldn't happen");
                cc->close_quietly();
            }
            cc = std::move(pit->second);
            log::debug(
                logcat,
                "{} connection to {}",
                replaced ? "Replaced existing" : "Established",
                rid.to_network_address(true));
        }

        pending_outbound.erase(pit);
    }

    void Endpoint::on_conn_established(quic::Connection& conn)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        router.loop.call([this, weak = conn.weak_from_this()]() {
            auto conn = weak.lock();
            if (not conn)
            {
                log::warning(logcat, "Connection died before connection open callback execution!");
                return;
            }

            if (conn->is_inbound())
                on_inbound_conn(std::move(conn));
            else
                on_outbound_conn(std::move(conn));
        });
    }

    void Endpoint::on_conn_closed(quic::Connection& conn, uint64_t ec)
    {
        if (conn.remote_key().size() != RouterID::SIZE)
        {
            log::debug(logcat, "on_conn_closed on rejected connection (ec={}), nothing to do", ec);
            return;
        }

        router.loop.call([this,
                          ref_id = conn.reference_id(),
                          rid = RouterID{conn.remote_key().first<32>()},
                          ec,
                          path = conn.path()] {
            if (auto it = relay_conns.find(rid); it != relay_conns.end())
            {
                auto& relcon = it->second;
                bool found = false;
                if (relcon.inbound && ref_id == relcon.inbound->conn->reference_id())
                {
                    relcon.close_quietly(true);
                    found = true;
                    log::debug(logcat, "Inbound connection from {} closed (ec={})", rid.to_network_address(true), ec);
                }
                if (relcon.outbound && ref_id == relcon.outbound->conn->reference_id())
                {
                    relcon.close_quietly(false);
                    found = true;
                    log::debug(logcat, "Outbound connection to {} closed (ec={})", rid.to_network_address(true), ec);
                }
                if (!relcon.conn)
                {
                    log::debug(logcat, "No remaining relay connection with {}!", rid.to_network_address(true));
                    relay_conns.erase(it);
                }
                if (found)
                {
                    relay_bidir.erase(rid);
                    return;
                }
            }
            if (auto it = client_conns.find(rid);
                it != client_conns.end() and ref_id == it->second->conn->reference_id())
            {
                log::debug(
                    logcat,
                    "Closed {} {} (ec={})",
                    router.is_service_node ? "client connection from" : "connection to",
                    rid.to_network_address(!router.is_service_node),
                    ec);
                client_conns.erase(it);
                return;
            }
            if (auto it = pending_outbound.find(rid); it != pending_outbound.end())
            {
                log::debug(
                    logcat,
                    "Pending connection to {} closed before establishing (ec={})",
                    rid.to_network_address(true),
                    ec);
                pending_outbound.erase(it);
                return;
            }

            log::warning(
                logcat,
                "Closed untracked connection to {} (ref_id={}, ec={})",
                rid.to_network_address(true /* don't know! */),
                ref_id,
                ec);
        });
    }

}  // namespace llarp::link
