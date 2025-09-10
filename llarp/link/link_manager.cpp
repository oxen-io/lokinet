#include "link_manager.hpp"

#include <llarp/constants/path.hpp>
#include <llarp/contact/contactdb.hpp>
#include <llarp/contact/router_id.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/messages/common.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/messages/fetch.hpp>
#include <llarp/messages/path.hpp>
#include <llarp/messages/session.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/path/path.hpp>
#include <llarp/path/transit_hop.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/bspan.hpp>
#include <llarp/util/random.hpp>
#include <llarp/util/time.hpp>

#include <nlohmann/json.hpp>
#include <oxen/quic/btstream.hpp>
#include <oxen/quic/context.hpp>
#include <oxen/quic/opt.hpp>
#include <oxenc/bt_producer.h>
#include <sodium/crypto_generichash_blake2b.h>
#include <sodium/randombytes.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <exception>
#include <ranges>

#ifndef LOKINET_EMBEDDED_ONLY
#include <llarp/rpc/rpc_client.hpp>
#endif

namespace llarp::link
{
    static auto logcat = llarp::log::Cat("link.manager");

    // These requests come over a path (as a "path_control" request),
    // we may or may not need to make a request to another relay,
    // then respond (onioned) back along the path.
    std::unordered_map<std::string_view, void (Manager::*)(quic::message, std::optional<std::string>)>
        Manager::path_requests = {
            {"path_control"sv, &Manager::handle_path_control},
            {"publish_cc"sv, &Manager::handle_publish_cc},
            {"find_cc"sv, &Manager::handle_find_cc},
            {"fetch_rcs"sv, &Manager::handle_fetch_rcs},
            {"fetch_rids"sv, &Manager::handle_fetch_router_ids},
            {"resolve_sns"sv, &Manager::handle_resolve_sns},
            {"session_init"sv, &Manager::handle_initiate_session},
            {"session_close"sv, &Manager::handle_close_session},
            {"path_switch"sv, &Manager::handle_path_switch},
            {"path_ping"sv, &Manager::handle_path_ping}};

    void Manager::register_commands(quic::BTRequestStream& s, const RouterID& remote_rid, bool client_only)
    {
        // TODO FIXME: registering all these commands on every stream feels icky; a quic fallback
        // handler could do this better.

        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        s.register_handler("path_control"s, [this](quic::message m) {
            router.loop.call([this, msg = std::move(m)]() mutable { handle_path_control(std::move(msg)); });
        });

        if (client_only)
        {
            log::trace(logcat, "Registered all client-only BTStream commands!");
            return;
        }

        s.register_handler("path_switch"s, [this](quic::message m) {
            router.loop.call([this, msg = std::move(m)]() mutable { handle_path_switch(std::move(msg)); });
        });

        s.register_handler("session_init"s, [this](quic::message m) {
            router.loop.call([this, msg = std::move(m)]() mutable { handle_initiate_session(std::move(msg)); });
        });

        s.register_handler("session_close"s, [this](quic::message m) {
            router.loop.call([this, msg = std::move(m)]() mutable { handle_close_session(std::move(msg)); });
        });

        s.register_handler("path_build"s, [this, remote_rid](quic::message m) {
            router.loop.call(
                [this, remote_rid, msg = std::move(m)]() mutable { handle_path_build(std::move(msg), remote_rid); });
        });

        s.register_handler("fetch_rcs"s, [this](quic::message m) {
            router.loop.call([this, msg = std::move(m)]() mutable { handle_fetch_rcs(std::move(msg)); });
        });

        s.register_handler("gossip_rc"s, [this](quic::message m) {
            router.loop.call([this, msg = std::move(m)]() mutable { handle_gossip_rc(std::move(msg)); });
        });

        s.register_handler("publish_cc"s, [this](quic::message m) {
            router.loop.call([this, msg = std::move(m)]() mutable { handle_publish_cc(std::move(msg)); });
        });

        s.register_handler("find_cc"s, [this](quic::message m) {
            router.loop.call([this, msg = std::move(m)]() mutable { handle_find_cc(std::move(msg)); });
        });

        s.register_handler("resolve_sns"s, [this](quic::message m) {
            router.loop.call([this, msg = std::move(m)]() mutable { handle_resolve_sns(std::move(msg)); });
        });

        log::trace(logcat, "Registered all commands for connection to remote RID:{}", remote_rid);
    }

    void Manager::register_bootstrap_commands(quic::BTRequestStream& s)
    {
        s.register_handler("bfetch_rcs"s, [this](quic::message m) {
            router.loop.call([this, msg = std::move(m)]() mutable { handle_fetch_bootstrap_rcs(std::move(msg)); });
        });

        log::trace(logcat, "Registered bootstrap commands for inbound bootstrap connection");
    }

    Manager::Manager(Router& r) : router{r}, endpoint{*this} {}

    // void Manager::close_connection(RouterID rid) { return ep->close_connection(rid); }

#if 0
    /*
     * TODO FIXME - fix reachability logic (see router.cpp)
     */
    void Manager::test_reachability(
        const RouterID& rid, connection_established_callback on_open, connection_closed_callback on_close)
    {
        if (auto rc = router.node_db().get_rc(rid))
            connect_to(*rc, std::move(on_open), std::move(on_close));
        else
            log::warning(logcat, "Could not find RelayContact for connection to rid:{}", rid);
    }
#endif

    void Manager::stop()
    {
        if (is_stopping.exchange(true))
            return;

        router.loop.call_get([this] { endpoint.shutdown(); });
    }

    Manager::~Manager() { stop(); }

    // TODO: this
    nlohmann::json Manager::extract_status() const { return {}; }

    void Manager::connect_to_keep_alive(int num_conns)
    {
        if (router.node_db().strict_connect_enabled())
        {
            assert(not router.is_service_node);

            // TESTNET: TODO: if given strict-connects, fetch their RCs SPECIFICALLY in bootstrapping
            // TODO FIXME: why?  That sounds rather metadata-leaky.
            log::warning(logcat, "FINISH STRICT CONNECT (SEE COMMENT)");
        }

        if (auto rcs = router.node_db().get_n_random_rcs(
                num_conns,
                true,
                [this](const RemoteRC& rc) {
                    return not router.link_endpoint().connected_to_relay(rc.router_id(), /*include_pending=*/true);
                });
            !rcs.empty())
            for (const auto* rc : rcs)
                endpoint.ensure_connection(*rc);
        else
            log::warning(logcat, "NodeDB query for {} random RCs for connection returned none", num_conns);
    }

    int Manager::gossip_rc(const RemoteRC& rc, const quic::ConnectionID* sender)
    {
        int count = 0;
        endpoint.for_each_relay_conn([&rc, &sender, &count](const RouterID& rid, link::Connection& conn) {
            // Don't gossip this to RC's origin, or back along the connection that sent it to us:
            if (rid == rc.router_id() or (sender && *sender == conn.conn->reference_id()))
                return;

            conn.control_stream->command("gossip_rc", rc.view());
            ++count;
        });

        return count;
    }

    void Manager::handle_gossip_rc(quic::message m)
    {
        RemoteRC rc;

        try
        {
            rc = RemoteRC{m.body(), router.netid()};
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Invalid gossipped RC: {}", e.what());
            return;
        }

        if (router.node_db().verify_store_gossip_rc(rc))
        {
            log::debug(
                logcat,
                "Received new or significantly updated RC for {}; gossipping to peers",
                rc.router_id().short_string());
            gossip_rc(rc, &m.conn_rid());
        }
        else
            log::debug(
                logcat,
                "Received known or minor RC update for {}; not gossipping to peers",
                rc.router_id().short_string());
    }

    void Manager::fetch_bootstrap_rcs(
        const RemoteRC& source, std::vector<std::byte> payload, std::function<void(quic::message)> func)
    {
        assert(router.loop.inside());
        endpoint.send_command(source, "bfetch_rcs", std::move(payload), std::move(func));
    }

    void Manager::handle_fetch_bootstrap_rcs(quic::message m)
    {
        // this handler should not be registered for clients
        assert(router.is_service_node);
        log::critical(logcat, "Handling bootstrap fetch request...");

        std::optional<RemoteRC> remote;
        int quantity;

        try
        {
            oxenc::bt_dict_consumer btdc{m.body()};
            if (btdc.skip_until("l"))
                remote.emplace(btdc.consume_dict_data(), router.netid());

            quantity = btdc.require<int>("q");
        }
        catch (const std::exception& e)
        {
            log::critical(logcat, "Exception handling bootstrap RC Fetch request (body:{}): {}", m.body(), e.what());
            m.respond(messages::ERROR_RESPONSE, true);
            return;
        }

        if (remote)
        {
            if (router.node_db().is_registered(remote->router_id()))
            {
                router.node_db().put_rc(*remote);
                log::debug(
                    logcat,
                    "Bootstrap node confirmed {} is registered; approving fetch request and saving RC!",
                    remote->router_id().to_network_address(true));
            }
            else
                log::debug(logcat, "Ignoring bootstrap fetch with unregistered RC from RID:{}", remote->router_id());
        }

        std::vector<std::string_view> rcs;
        if (quantity == 0)
        {
            // 0 means "all"
            auto& src = router.node_db().get_known_rcs();

            rcs.reserve(src.size());
            auto now = llarp::time_now_ms();
            for (const auto& rc : std::views::values(src))
                if (not rc.is_expired(now))
                    rcs.push_back(rc.view());

            std::ranges::shuffle(rcs, llarp::csrng);
        }
        else
        {
            rcs.reserve(quantity);
            for (auto* rc : router.node_db().get_n_random_rcs(quantity))
                rcs.push_back(rc->view());
        }

        if (rcs.empty())
        {
            m.respond("No RCs", true);
            return;
        }

        size_t reserve = 7;  // d1:rl...ee  (not counting the "...")
        for (auto& rc : rcs)
            reserve += rc.size();  // Pre-encoded bt data, so no additional overhead

        oxenc::bt_dict_producer btdp;
        btdp.reserve(reserve);
        {
            auto rc_list = btdp.append_list("r");
            for (const auto& rc : rcs)
                rc_list.append_encoded(rc);
        }
        m.respond(std::move(btdp).str());
    }

    void Manager::handle_fetch_rcs(quic::message m, std::optional<std::string> inner_body)
    {
        log::debug(logcat, "Handling FetchRC request...");
        // this handler should not be registered for clients
        assert(router.is_service_node);

        std::unordered_set<RouterID> explicit_ids;

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
                if (auto* maybe_rc = router.node_db().get_rc(rid))
                {
                    sublist.append_encoded(maybe_rc->view());
                    ++count;
                }
            }
            log::info(logcat, "Returning {} RCs for FetchRC request...", count);
        }

        m.respond(std::move(btdp).str());
    }

    void Manager::handle_fetch_router_ids(quic::message m, std::optional<std::string>)
    {
        log::trace(logcat, "Handling FetchRIDs request...");
        // this handler should not be registered for clients
        assert(router.is_service_node);

        auto known_rids = router.node_db().get_registered_relays();
        oxenc::bt_dict_producer btdp;

        {
            auto btlp = btdp.append_list("r");

            for (const auto& rid : known_rids)
                btlp.append(rid.to_view());
        }

        log::debug(logcat, "Returning ALL ({}) locally held RIDs to FetchRIDs request!", known_rids.size());
        m.respond(std::move(btdp).str());
    }

    void Manager::handle_resolve_sns(
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

        router.rpc_client()->lookup_sns_hash(
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

    void Manager::handle_publish_cc(quic::message m, std::optional<std::string> inner_body)
    {
        log::trace(logcat, "Received request to publish client contact!");

        EncryptedClientContact enc;
        std::optional<int> location;

        try
        {
            std::tie(enc, location) =
                PublishClientContact::deserialize(oxenc::bt_dict_consumer{inner_body ? *inner_body : m.body()});
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

        if (not router.is_service_node)
        {
            // If we aren't a service node then this message is presumably a pushed introset update
            // pushed to us by someone who we should already have an outbound connection with.

            // TODO FIXME: This previously included an optional "i" key containing the sender for
            // these send-over-session messages, but that seems dumb because 1) it isn't
            // authenticated, and 2) we should already *know* the sender based on the session the
            // message arived on.

            log::critical(logcat, "TODO FIXME STAGENET TOTHINK: fix incoming session CC handling");
            m.respond("FIXME!", true);

#if 0
            if (not sender.has_value())
            {
                log::warning(logcat, "Received new EncryptedClientContact from path control with no sender!");
                // TODO FIXME - does this client-to-client push actually need a response?
                return m.respond(messages::ERROR_RESPONSE, true);
            }

            NetworkAddress sender_addr{*sender, true};
            auto session = router.session_endpoint().get_session(sender_addr);
            if (!session || !session->is_outbound)
            {
                log::warning(logcat, "Ignoring pushed ClientContact from {}: no outbound session found", sender_addr);
                return m.respond(messages::ERROR_RESPONSE, true);
            }

            auto intro = enc.decrypt(*sender);
            if (not intro)
                // error message already logged in decrypt(...) call
                return m.respond(messages::ERROR_RESPONSE, true);

            log::debug(logcat, "Storing ClientContact for remote {}", sender_addr);
            router.contact_db().put_cc(std::move(enc));

            // FIXME: this should probably come encrypted.  Need to encrypt it and also handle it here.

            return m.respond(messages::OK_RESPONSE);
#endif
        }

        auto cc_blind_pk = enc.key();

        // These messages have two steps: the client sends each message down a path with a 0-3
        // location value indicating which of the 4 closest locations it should be published to.
        // The relay receiving it then determines the target relay (based on the input) and forwards
        // it along.  (Or, if it got lucky and is the requested index, stores it directly).
        //
        // The forwarded step here does *not* include the position, and must be a relay-to-relay
        // direct message: the receiver of this direct message stores it if they are in the top-4+1
        // locations (the extra +1 is to allow for a slight amount of drift in positions, e.g. in
        // case of races with oxen block changes or other stale data).
        //
        // This two-step process helps ensure that publishes work even if the client has an
        // incomplete or outdated set of RCs, and doesn't require the client to build extra paths to
        // the 4 publish locations.
        const bool is_forwarded = not inner_body;

        auto closest_rids = router.node_db().find_many_closest_to(cc_blind_pk, path::CC_PUBLISH_LOCATIONS + 1);
        if (closest_rids.size() < path::CC_PUBLISH_LOCATIONS)
        {
            m.respond("No RCs available!", true);
            return;
        }

        if (!is_forwarded)
        {
            if (!location || *location < 0 || *location >= path::CC_PUBLISH_LOCATIONS)
            {
                log::warning(
                    logcat,
                    "Ignoring ECC publish from a client with {} publish index",
                    location ? "invalid ({})"_format(*location) : "missing");
                m.respond(
                    messages::serialize_status_response(
                        location ? "INVALID PUBLISH LOCATION" : "MISSING PUBLISH LOCATION"),
                    true);
                return;
            }

            const auto& rid = closest_rids[*location];

            if (rid == router.local_rid())
            {
                // Special case: we *are* the intended location
                router.contact_db().put_cc(std::move(enc));
                m.respond(messages::OK_RESPONSE);
                return;
            }

            log::debug(
                logcat,
                "Received PublishClientContact (key: {}, index: {}); forwarding to {}",
                enc.key(),
                *location,
                rid);

            endpoint.send_command(
                rid,
                "publish_cc",
                PublishClientContact::serialize(std::move(enc)),
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
            return;
        }

        // Otherwise this was forwarded, so we store it if and only if we are one of the
        // CC_PUBLISH_LOCATIONS closest locations, and we don't forward regardless.
        //
        // We don't require that we were strictly in the correct position that the client originally
        // sent (and thus we don't even include the target location when forwarding), because an
        // Oxen block update with a new or removed registration at just the wrong time could shift
        // indices, and we still want to store it even if we shifted (e.g. from 3nd to 2nd).
        for (auto& rid : closest_rids)
            if (rid == router.local_rid())
            {
                router.contact_db().put_cc(std::move(enc));
                m.respond(messages::OK_RESPONSE);
                return;
            }

        log::warning(
            logcat, "Ignoring forwarded CC publish: we are not in the top {} publish locations", closest_rids.size());
        m.respond(messages::ERROR_RESPONSE, true);
        return;
    }

    void Manager::handle_find_cc(quic::message m, std::optional<std::string> inner_body)
    {
        log::trace(logcat, "Received request to find client contact!");

        PubKey blinded_pubkey;
        try
        {
            blinded_pubkey =
                FindClientContact::deserialize(oxenc::bt_dict_consumer{inner_body ? *inner_body : m.body()});
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception: {}", e.what());
            return m.respond(messages::ERROR_RESPONSE, true);
        }

        auto closest_rids = router.node_db().find_many_closest_to(blinded_pubkey, path::CC_PUBLISH_LOCATIONS);
        if (closest_rids.size() < path::CC_PUBLISH_LOCATIONS)
            return m.respond("No RCs!", true);

        // We don't provide the answer ourselves unless we are in the closest-4 set because it's
        // possible we *were* in the closest 4 but then dropped out, but still have a stale record
        // hanging around.
        auto authoritative = std::ranges::count(closest_rids, router.local_rid());
        assert(authoritative <= 1);

        if (authoritative)
        {
            // TODO FIXME: Do we want to send the requests off to other relays *even if* we have it,
            // to double-check against other relays in case ours is stale?

            if (auto maybe_cc = router.contact_db().get_encrypted_cc(blinded_pubkey))
            {
                log::info(
                    logcat,
                    "Received FindClientContact request (key: {}); returning local EncryptedClientContact...",
                    blinded_pubkey);
                return m.respond(FindClientContact::serialize_response(*maybe_cc));
            }

            log::debug(
                logcat,
                "Received FindClientContact and we are authoritative, but don't have a matching CC for {}",
                blinded_pubkey);
            // Don't return an error because we can still possibly forward it to other authoritative
            // nodes, below, and it's perfectly possible for us not to have it if we missed it for
            // various reasons.
        }

        // If the optional was nullopt, then this was a relay <-> relay request. As a result, we should NOT
        // allow it to continue propagating
        if (not inner_body)
        {
            log::critical(
                logcat,
                "Received relayed FindClientContact request (key: {}); could not find locally, relaying "
                "error...",
                blinded_pubkey);
            return m.respond(FindClientContact::NOT_FOUND, true);
        }

        auto remaining = std::make_shared<size_t>(closest_rids.size() - authoritative);
        auto hook = [m = std::move(m), remaining](quic::message msg) mutable {
            if (*remaining == 0)
                return;  // Already answered by an earlier response

            if (msg)
            {
                *remaining = 0;
                log::info(logcat, "Relayed FindClientContact request SUCCEEDED! Relaying response");
                log::trace(logcat, "Relayed FindClientContact response: {}", buffer_printer{msg.body()});
                m.respond(msg.body());
                return;
            }

            if (--*remaining == 0)
                return;  // This was an error, but there are more responses to come back

            log::warning(logcat, "All FindClientContact requests FAILED! Relaying failure");
            m.respond(msg.timed_out ? messages::TIMEOUT_RESPONSE : msg.body(), true);
        };

        log::debug(logcat, "Relaying FindClientContactMessage (key: {}) to {} peers", blinded_pubkey, *remaining);

        auto forwarded_find_cc = FindClientContact::serialize(blinded_pubkey);
        for (const auto& rid : closest_rids)
        {
            if (rid == router.local_rid())
                continue;
            endpoint.send_command(rid, "find_cc", forwarded_find_cc, hook);
        }
    }

    void Manager::handle_path_build(quic::message m, const RouterID& from)
    {
        if (!router.path_context.is_transit_allowed())
        {
            log::warning(logcat, "got path build request when not permitting transit");
            return m.respond(PATH::BUILD::NO_TRANSIT, true);
        }

        try
        {
            auto frames_in = m.body<std::byte>();

            if (frames_in.size() != path::BUILD_LENGTH * path::BUILD_FRAME_SIZE)
            {
                log::info(
                    logcat,
                    "Ignoring path build with invalid length {} != expected {}*{}",
                    frames_in.size(),
                    path::BUILD_LENGTH,
                    path::BUILD_FRAME_SIZE);
                m.respond(PATH::BUILD::BAD_FRAMES, true);
                return;
            }

            auto now = llarp::time_now_ms();
            auto [hop, dh_nonce] =
                path::PathHandler::decrypt_build_frame(frames_in.first<path::BUILD_FRAME_SIZE>(), router, from, now);

            if (hop->expiry > now + path::MAX_LIFETIME || hop->expiry <= now)
                throw path::TransitHopError::INVALID_LIFETIME();

            if (router.path_context.has_transit_hop(hop->rxid) || router.path_context.has_transit_hop(hop->txid))
                throw path::TransitHopError::HOP_ID_UNAVAILABLE();

            // we are terminal hop and everything is okay
            if (hop->terminal_hop)
            {
                log::info(logcat, "We are the terminal hop; path build succeeded");
                router.path_context.put_transit_hop(std::move(hop));
                return m.respond(messages::OK_RESPONSE);
            }

            // rotate remaining frames forward
            std::vector<std::byte> frames;
            frames.resize(frames_in.size());
            std::memcpy(
                frames.data(), frames_in.data() + path::BUILD_FRAME_SIZE, frames_in.size() - path::BUILD_FRAME_SIZE);
            // and then fill the frame at the end (where ours would rotate to) with random junk:
            random_fill(std::span{frames}.last(path::BUILD_FRAME_SIZE));

            // De-onion the remaining frames (not including the known junk frame at the end) for the next hop
            crypto::xchacha20(
                std::span{frames}.first((path::BUILD_LENGTH - 1) * path::BUILD_FRAME_SIZE),
                hop->shared_secret,
                dh_nonce ^ hop->xor_nonce);

            const auto& upstream = hop->upstream;

            endpoint.send_command(
                upstream,
                "path_build",
                std::move(frames),
                [this, hop = std::move(hop), prev_message = std::move(m)](quic::message m) mutable {
                    if (m)
                    {
                        log::info(
                            logcat,
                            "Upstream returned successful path build response; locally storing Hop ({}) and "
                            "relaying",
                            *hop);
                        router.path_context.put_transit_hop(std::move(hop));
                        prev_message.respond(messages::OK_RESPONSE);
                        return;
                    }

                    log::info(
                        logcat, "Upstream ({}) path build {}", hop->upstream, m.timed_out ? "timed out" : "failed");

                    if (m.is_error())
                        prev_message.respond(m.body(), m.is_error());
                    // else leave it unanswered so that it times out at the request origin
                });
        }
        catch (const path::TransitHopError& e)
        {
            log::warning(logcat, "An error occured during path build request handling: {}", e.what());
            return m.respond(messages::serialize_status_response(e.error_code), true);
        }
    }

    void Manager::handle_path_control(quic::message m, std::optional<std::string> inner_body)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        HopID hop_id;
        std::vector<std::byte> payload;
        SymmNonce nonce;

        auto body = inner_body ? as_bspan(*inner_body) : m.body<std::byte>();

        if (body.size() <= path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD)
        {
            m.respond(messages::ERROR_RESPONSE, true);
            return;
        }

        payload.assign(body.begin(), body.end());
        static_assert(path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD == SymmNonce::SIZE + HopID::SIZE + 1);
        auto [inner_payload, bnonce, bhop, msgtype] = split_span_tail<SymmNonce::SIZE, HopID::SIZE, 1>(payload);

        if (msgtype[0] != std::byte{0x01})
        {
            log::warning(
                logcat, "Invalid/unknown path_control encrypted message type {}", static_cast<int>(msgtype[0]));
            log::trace(logcat, "Failed path_control payload: {}", buffer_printer{body});
            m.respond(messages::ERROR_RESPONSE, true);
            return;
        }

        nonce.assign(bnonce);
        hop_id.assign(bhop);

        if (not router.is_service_node)
        {
            auto path = router.path_context.get_path(hop_id);

            if (not path)
            {
                log::warning(logcat, "Client received path control with unknown rxID: {}", hop_id);
                m.respond(messages::ERROR_RESPONSE, true);
                return;
            }

            log::trace(logcat, "Received path control for local client: {}", buffer_printer{inner_payload});

            for (auto& hop : path->hops)
                crypto::xchacha20(inner_payload, hop.shared_secret, nonce);

            handle_path_request(std::move(m), inner_payload);
            return;
        }

        auto hop = router.path_context.get_transit_hop_ptr(hop_id);

        if (not hop)
        {
            log::warning(logcat, "Received path control for unknown path (hop ID: {})", hop_id);
            m.respond(messages::ERROR_RESPONSE, true);
            return;
        }

        nonce ^= hop->xor_nonce;
        crypto::xchacha20(inner_payload, hop->shared_secret, nonce);

        if (not inner_body)
        {
            // if terminal hop, payload should contain a request (e.g. "sns_resolve"); handle and respond.
            if (hop->terminal_hop)
            {
                log::debug(logcat, "We are terminal hop for path request: {}", *hop);
                handle_path_request(std::move(m), inner_payload);
                return;
            }

            log::debug(logcat, "We are intermediate hop for path request: {}", *hop);
        }
        else
        {
            log::debug(logcat, "We are bridge node for aligned path request ({})! Forwarding downstream", *hop);
            log::trace(logcat, "Payload: {}", buffer_printer{*inner_body});
        }

        auto next = hop->next_id(hop_id);

        if (not next)
        {
            log::warning(logcat, "Failed to query hop ({}) for next ids (input: {})", *hop, hop_id);
            return m.respond(messages::ERROR_RESPONSE, true);
        }

        const auto& [next_rid, next_hopid] = *next;

        // We're relaying this message down a path, and we've already done our decryption to the
        // inner_payload so now we just need to replace the nonce and next hop ID in the outer
        // payload before passing it along:
        nonce.copy_to(bnonce);
        next_hopid.copy_to(bhop);

        endpoint.send_command(
            next_rid,
            "path_control",
            std::move(payload),
            [hop_weak = std::weak_ptr{hop}, hop_id, prev_message = std::move(m)](quic::message response) mutable {
                auto hop = hop_weak.lock();
                if (not hop)
                {
                    log::warning(logcat, "Received response to path control message with non-existent TransitHop!");
                    return prev_message.respond(messages::ERROR_RESPONSE, true);
                }

                if (response.timed_out)
                {
                    log::warning(logcat, "Path control message response timed out");
                    // There's no real point in sending a failure response here because the
                    // originator is using the same timeout and is going to time out right around
                    // the same time, so any response we might sent isn't going to be useful (and
                    // would be treated no differently than the originator hitting their own
                    // timeout).
                    return;
                }

                if (response)
                    log::debug(logcat, "Path control message returned successfully");
                else
                    log::warning(logcat, "Path control message returned an error!");

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

    // FIXME: overhead for session MAC?
    static constexpr size_t MIN_PATH_DATA_MESSAGE_SIZE = 0 /*payload*/ + 1 /*packet type*/ + sizeof(HopID) /*pivot*/
        + path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD /*nonce, hop, type*/;

    // Removes the message type byte, HopID, and SymmNonce from the end of a path message, returning
    // the hopid and nonce.  The vector is resized to drop the loaded values (and thus will contain
    // only the onioned payload after this call).
    //
    // Warns and returns nullopt if the input vector is too short or the message type byte is
    // invalid (and thus the message should be dropped).
    static std::optional<std::pair<HopID, SymmNonce>> extract_path_message_metadata(std::vector<std::byte>& message)
    {
        if (message.size() < MIN_PATH_DATA_MESSAGE_SIZE)
        {
            log::warning(logcat, "Dropping invalid too-short path data message");
            return std::nullopt;
        }

        // Deliberately break compilation if data message overhead changes in path without getting
        // updated here as well:
        static_assert(path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD == SymmNonce::SIZE + HopID::SIZE + 1);

        // For the detailed structure of this encoding, see description in session/session.cpp
        std::byte msgtype = message.back();
        if (msgtype != std::byte{0x01})
        {
            log::warning(logcat, "Dropping data message with invalid msgtype {}", std::to_integer<int>(msgtype));
            return std::nullopt;
        }
        message.pop_back();

        std::optional<std::pair<HopID, SymmNonce>> result;
        auto& [hop_id, nonce] = result.emplace();
        hop_id.assign(std::span{message}.last<HopID::SIZE>());
        message.resize(message.size() - HopID::SIZE);

        nonce.assign(std::span{message}.last<SymmNonce::SIZE>());
        message.resize(message.size() - SymmNonce::SIZE);

        return result;
    }

    void Manager::handle_path_data_message(std::vector<std::byte> message)
    {
        auto maybe_hop_nonce = extract_path_message_metadata(message);
        if (!maybe_hop_nonce)
            return;
        auto& [hop_id, nonce] = *maybe_hop_nonce;

        // The remainder of `message` is onion-encrypted.

        // We've received a data message down a path.  There are four possible cases to consider
        // here:
        //
        // 1. We are a client and thus the final destination of the message.  We consume it.
        //
        // 2. We are a relay and are the path terminus and the target (i.e. a relay session data
        //    message).  We consume it.
        //
        // 3. We are a relay and are the path terminus and the message is to pivot to another path.
        //    We onion decrypt, then read the pivot it, then onion encrypt for the target aligned
        //    path and send it along.
        //
        // 4. We are a relay along the path but *not* the terminal.  We apply one onion layer and
        //    pass it along to the next hop.

        // Case 1: client
        if (not router.is_service_node)
        {
            auto path = router.path_context.get_path(hop_id);

            if (not path)
            {
                log::warning(logcat, "Client received path data with unknown rxID: {}", hop_id);
                return;
            }

            // We're receiving this down an aligned path, which means each hop applied xchacha and
            // nonce mutation so we run through the hops and apply the reverse operation,
            // repeatedly, in order from nearest (most recently encrypted) back to terminus:
            for (auto& hop : path->hops)
            {
                crypto::xchacha20(message, hop.shared_secret, nonce);
                nonce ^= hop.xor_nonce;
            }

            // Client-bound session data has no pivot, just [encrypted][sessiontag], so we extract
            // and remove the session tag then give the remainder for be session-decrypted.  The
            // nonce (after the above mutations) also matches the nonce we want to use for the
            // session encryption.
            // FIXME: poly1305 mac goes in here somewhere too!
            session_tag tag;
            tag.assign(std::span{message}.last<session_tag::SIZE>());
            message.resize(message.size() - session_tag::SIZE);

            log::trace(logcat, "Handling incoming data message at the client end of a path");
            return handle_session_data(std::move(message), tag, nonce);
        }

        // Cases 2-4: relay.
        auto hop = router.path_context.get_transit_hop(hop_id);
        if (not hop)
        {
            log::warning(logcat, "Received path data with unknown next hop (ID: {})", hop_id);
            return;
        }

        // All cases first apply a nonce mutation and then one onion encrypt/decrypt:
        nonce ^= hop->xor_nonce;
        crypto::xchacha20(message, hop->shared_secret, nonce);

        if (hop->terminal_hop)
        {
            // Case 2 or 3:
            log::trace(logcat, "We are terminal hop for path data");

            // What's left in message after the above xchacha is back to what the data message
            // creator set up for us: [ENCRYPTED, SESSION_TAG, PIVOT_ID].

            auto [payload, bsession_tag, bpivot_id] = split_span_tail(message, session_tag::SIZE, HopID::SIZE);

            HopID pivot_id;
            pivot_id.assign(bpivot_id.first<HopID::SIZE>());

            // Identify whether we are in case 2 (relay session) or 3 (pivot) by seeing whether we
            // were told to "pivot" to the identical path (which is a special condition used
            // explicitly for session data messages):
            if (pivot_id == hop_id)
            {
                // Case 2: this is a session data message to this relay; extract the session tag and
                // then drop everything down to the session payload for handle_session_data to deal
                // with.
                session_tag tag;
                tag.assign(bsession_tag.first<session_tag::SIZE>());
                message.resize(payload.size());

                log::trace(logcat, "Incoming data message is a relay session data message");
                handle_session_data(std::move(message), tag, nonce);
                return;
            }

            // Case 3: we are pivoting the message down another path:
            //
            auto trans_hop = router.path_context.get_transit_hop(pivot_id);
            if (not trans_hop)
            {
                log::warning(logcat, "Terminal hop received path data message with unknown pivot id: {}", pivot_id);
                return;
            }

            // We don't want the pivot_id anymore, and don't want to include it in the back-side
            // relay->client path, so drop it off the back of the message, leaving the session-encrypted-payload +
            // session tag in place.  However, we *also* need to bebuild this into a path message suitable for sending
            // down the back path, so we also need to add the path encryption bits:
            message.resize(message.size() - HopID::SIZE + path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD);

            // This is essentially a single-iteration version of Path::encrypt_path_data_message,
            // except that because this is going "backwards" (from the perspective of a client), the
            // xor happens *before* the xchacha.

            static_assert(path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD == SymmNonce::SIZE + HopID::SIZE + 1);
            auto [session_payload, bnonce, bhop, msgtype] = split_span_tail<SymmNonce::SIZE, HopID::SIZE, 1>(message);

            nonce ^= trans_hop->xor_nonce;
            crypto::xchacha20(session_payload, trans_hop->shared_secret, nonce);

            nonce.copy_to(bnonce);
            pivot_id.copy_to(bhop);
            msgtype[0] = std::byte{0x01};

            log::trace(logcat, "Pivoting message down another path");
            endpoint.send_datagram(trans_hop->downstream, std::move(message));
            return;
        }

        // Case 4: we're an intermediate so we forward it to the next hop
        auto next = hop->next_id(hop_id);
        if (not next)
        {
            // Log error severity because it shouldn't be possible if we found the hop in
            // the first place
            log::error(logcat, "No next hop found in transit hop?!");
            return;
        }
        auto& [next_rid, next_hopid] = *next;

        // We chopped off the 0x01, hop_id, and nonce at the top of this function, but now lets put
        // the new ones back on to make it suitable for the next hop.  (We're just resizing a vector
        // down and back up, so there's no reallocation or copying happening by the resizing and
        // little point in trying to avoid it).
        static_assert(path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD == SymmNonce::SIZE + HopID::SIZE + 1);
        message.resize(message.size() + path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD);
        auto [enc_data, bnonce, bhop, bmsgtype] = split_span_tail<SymmNonce::SIZE, HopID::SIZE, 1>(message);
        nonce.copy_to(bnonce);
        next_hopid.copy_to(bhop);
        bmsgtype[0] = std::byte{0x01};

        endpoint.send_datagram(next_rid, std::move(message));
    }

    void Manager::handle_session_data(std::vector<std::byte>&& payload, const session_tag& tag, const SymmNonce& nonce)
    {
        if (auto session = router.session_endpoint().get_session(tag))
            session->recv_session_data_message(std::move(payload), nonce);
        else
            log::warning(logcat, "Could not find session {} to receive session data message!", tag);
    }

    void Manager::handle_path_request(quic::message m, std::span<const std::byte> payload)
    {
        std::string endpoint, body;

        try
        {
            std::tie(endpoint, body) = PATH::CONTROL::deserialize(oxenc::bt_dict_consumer{payload});

            if (router.is_service_node and endpoint == "path_control")
            {
                log::info(logcat, "Received path control relay request; deserializing intermediate payload...");
                auto [_, i_body] = PATH::CONTROL::deserialize(oxenc::bt_dict_consumer{std::move(body)});
                return handle_path_control(std::move(m), std::move(i_body));
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
            (this->*(it->second))(std::move(m), std::move(body));
        }
        else
            log::warning(logcat, "Received path control request (`{}`), which has no local handler!", endpoint);
    }

    void Manager::handle_initiate_session(quic::message m, std::optional<std::string> inner_body)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        InitiateSession::Parameters params;

        try
        {
            if (inner_body)
            {
                params = InitiateSession::decrypt_deserialize(oxenc::bt_dict_consumer{*inner_body}, router.identity());
            }
            else  // TESTNET: this route is superfluous for this type of request almost surely, revisit soon
                params = InitiateSession::decrypt_deserialize(oxenc::bt_dict_consumer{m.body()}, router.identity());
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Failed to parse initiate session message: {}", e.what());
            return;
        }

        if (params.remote.router_id() == router.local_rid())
        {
            log::warning(logcat, "Received request to initiate session from local instance; ignoring!");
            return m.respond(InitiateSession::BAD_ADDRESS, true);
        }

        if (params.auth_token and not router.session_endpoint().validate(params.remote, params.auth_token))
        {
            log::warning(logcat, "Failed to authenticate session initiation request from remote:{}", params.remote);
            return m.respond(InitiateSession::AUTH_ERROR, true);
        }

        std::optional<session_tag> tag;

        if (router.is_service_node)
        {
            if (params.local_pivot_txid != params.remote_pivot_txid)
            {
                log::warning(logcat, "Received misrouted path-request to initiate client<->client session...");
                return m.respond(InitiateSession::BAD_ROUTE, true);
            }

            auto hop = router.path_context.get_transit_hop_ptr(params.local_pivot_txid);
            if (not hop)
            {
                log::warning(
                    logcat,
                    "Received path-request to initiate session with unknown hop (ID: {})",
                    params.local_pivot_txid);
                return m.respond(InitiateSession::BAD_ROUTE, true);
            }

            if (not hop->terminal_hop)
            {
                log::warning(
                    logcat,
                    "Received path-request to initiate session and we are NOT terminal hop (ID: {})",
                    params.local_pivot_txid);
                return m.respond(InitiateSession::BAD_ROUTE, true);
            }

            tag = router.session_endpoint().create_inbound_session(
                params.remote, params.remote_pivot_txid, std::move(hop), std::move(params.session_key));
        }
        else
        {
            auto* path = router.path_context.get_path(params.local_pivot_txid);
            if (not path)
            {
                log::warning(
                    logcat,
                    "Failed to find local path for new inbound session over pivot txid: {}",
                    params.local_pivot_txid);
                return m.respond(InitiateSession::BAD_ROUTE, true);
            }

            tag = router.session_endpoint().create_inbound_session(
                params.remote, params.remote_pivot_txid, path->shared_from_this(), std::move(params.session_key));
        }

        if (tag)
        {
            log::debug(
                logcat,
                "Inbound{}Session (tag:{}) created successfully!",
                router.is_service_node ? "Relay" : "Client",
                *tag);
            // FIXME: encryption
            return m.respond(InitiateSession::serialize_response(*tag));
        }

        log::warning(logcat, "Failed to configure InboundSession!");

        m.respond(messages::ERROR_RESPONSE, true);
    }

    void Manager::handle_path_switch(quic::message m, std::optional<std::string> inner_body)
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

        if (!router.is_service_node)
        {
            auto path = router.path_context.get_path(local_pivot_txid);

            if (not path)
            {
                log::warning(
                    logcat, "Received path-switch request for unknown local path (pivot txid:{})", local_pivot_txid);
                return m.respond(SessionPathSwitch::BAD_ID, true);
            }

            if (router.session_endpoint().recv_path_switch(tag, std::move(remote_pivot_txid), path->terminal_hopid()))
                return m.respond(messages::OK_RESPONSE);
        }
        else
        {
            auto hop = router.path_context.get_transit_hop_ptr(local_pivot_txid);

            if (not hop)
            {
                log::warning(
                    logcat, "Received path-switch request for unknown local hop (pivot txid:{})", local_pivot_txid);
                return m.respond(SessionPathSwitch::BAD_ID, true);
            }

            if (router.session_endpoint().recv_path_switch(tag, std::move(remote_pivot_txid), std::move(hop)))
                return m.respond(messages::OK_RESPONSE);
        }

        log::warning(logcat, "Received path-switch request for unknown session (tag:{})", tag);
        return m.respond(SessionPathSwitch::BAD_TAG, true);
    }

    void Manager::handle_path_ping(quic::message m, std::optional<std::string>)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        m.respond(messages::OK_RESPONSE);
    }

    void Manager::handle_close_session(quic::message m, std::optional<std::string> inner_body)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        // No reply expected from this endpoint.

        session_tag tag;

        try
        {
            if (inner_body)
                tag = CloseSession::deserialize(oxenc::bt_dict_consumer{*inner_body});
            else
                tag = CloseSession::deserialize(oxenc::bt_dict_consumer{m.body()});

            // TODO FIXME: we should be verifying where this came from so that someone can't close
            // someone else's tag.  (Perhaps some extra encrypted/signed data in the close session
            // message?).

            router.session_endpoint().close_session(tag, false);
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception: {}", e.what());
        }
    }

    void Manager::handle_path_latency(quic::message m)
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

    void Manager::handle_path_latency_response(quic::message m)
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

}  // namespace llarp::link
