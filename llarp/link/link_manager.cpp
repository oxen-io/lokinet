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
#include <llarp/session/session.hpp>
#include <llarp/util/bspan.hpp>
#include <llarp/util/random.hpp>
#include <llarp/util/time.hpp>
#include <llarp/util/zstd.hpp>

#include <nlohmann/json.hpp>
#include <oxen/quic/btstream.hpp>
#include <oxen/quic/connection_ids.hpp>
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
#include <llarp/rpc/oxend_rpc.hpp>
#endif

namespace llarp::link
{
    static auto logcat = llarp::log::Cat("link.manager");

    using session::session_tag;

    // These requests come over a path (as a "path_control" request),
    // we may or may not need to make a request to another relay,
    // then respond (onioned) back along the path.
    std::unordered_map<
        std::string_view,
        void (Manager::*)(std::span<const std::byte> payload, std::function<void(std::string)> respond)>
        Manager::path_requests = {
            {"publish_cc"sv, &Manager::handle_path_publish_cc},
            {"find_cc"sv, &Manager::handle_path_find_cc},
            {"fetch_rcs"sv, &Manager::handle_path_fetch_rcs},
            {"fetch_rids"sv, &Manager::handle_path_fetch_router_ids},
            {"resolve_sns"sv, &Manager::handle_path_resolve_sns},
            {"path_ping"sv, &Manager::handle_path_ping}};

    void Manager::handle_direct_request(
        void (Manager::*handler)(std::span<const std::byte>, std::function<void(std::string)>, bool), quic::message m)
    {
        auto body_str = m.body<std::byte>();
        auto resp = [msg = std::move(m)](std::string response) { msg.respond(response, false); };
        (this->*handler)(body_str, resp, true);
    }

    void Manager::register_commands(quic::BTRequestStream& s, const std::variant<RouterID, quic::ConnectionID>& remote)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        s.register_handler("path_control"s, [this](quic::message m) {
            router.loop.call([this, msg = std::move(m)]() mutable { handle_path_control(std::move(msg)); });
        });

        s.register_handler("session_control"s, [this](quic::message m) {
            router.loop.call([this, msg = std::move(m)]() mutable { handle_path_session_control(std::move(msg)); });
        });

        if (not router.is_service_node)
        {
            log::trace(logcat, "Registered all client-only BTStream commands!");
            return;
        }

        s.register_handler("path_build"s, [this, remote](quic::message m) {
            router.loop.call(
                [this, remote, msg = std::move(m)]() mutable { handle_path_build(std::move(msg), remote); });
        });

        s.register_handler("fetch_rcs"s, [this](quic::message m) {
            router.loop.call([this, msg = std::move(m)]() mutable {
                handle_direct_request(&Manager::handle_fetch_rcs, std::move(msg));
            });
        });

        s.register_handler("gossip_rc"s, [this](quic::message m) {
            router.loop.call([this, msg = std::move(m)]() mutable { handle_gossip_rc(std::move(msg)); });
        });

        s.register_handler("publish_cc"s, [this](quic::message m) {
            router.loop.call([this, msg = std::move(m)]() mutable {
                handle_direct_request(&Manager::handle_publish_cc, std::move(msg));
            });
        });

        s.register_handler("find_cc"s, [this](quic::message m) {
            router.loop.call([this, msg = std::move(m)]() mutable {
                handle_direct_request(&Manager::handle_find_cc, std::move(msg));
            });
        });

        // Endpoint called to test connectivity by other relays during relay testing.  It simply
        // replies with "pong" (we don't actually need a loop transfer here for the reply, but do it anyway so
        // that ping requests check that our router loop isn't stuck).
        s.register_handler("ping"s, [this](quic::message m) {
            router.loop.call([this, m = std::move(m)] {
                m.respond("pong");
                router.on_test_ping();
            });
        });
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
        auto rcs = router.node_db().get_n_random_edge_rcs(
            num_conns, false /* shuffling not needed */, [this](const RelayContact& rc) {
                return not router.link_endpoint().connected_to_relay(rc.router_id(), /*include_pending=*/true);
            });

        for (const auto* rc : rcs)
            endpoint.ensure_connection(*rc);

        if (rcs.empty())
            log::debug(logcat, "NodeDB query for {} edge RCs returned none", num_conns);
    }

    int Manager::gossip_rc(const RelayContact& rc, const quic::ConnectionID* sender)
    {
        int count = 0;
        endpoint.for_each_relay_conn([&rc, &sender, &count](const RouterID& rid, link::Connection& conn) {
            // Don't gossip this to RC's origin, or back along the connection that sent it to us:
            if (rid == rc.router_id() or (sender and *sender == conn.conn->reference_id()))
                return;

            conn.control_stream->command("gossip_rc", rc.view());
            ++count;
        });

        return count;
    }

    void Manager::handle_gossip_rc(quic::message m)
    {
        RelayContact rc;

        try
        {
            rc = RelayContact{m.body(), router.netid()};
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

    void Manager::handle_fetch_bootstrap_rcs(quic::message m)
    {
        // this handler should not be registered for clients
        assert(router.is_service_node);
        log::info(logcat, "Handling bootstrap fetch request");

        std::vector<std::string_view> rcs;
        rcs.push_back("l"sv);
        auto& src = router.node_db().get_known_rcs();

        rcs.reserve(src.size());
        auto now = llarp::time_now_ms();
        for (const auto& rc : std::views::values(src))
            if (not rc.is_expired(now))
                rcs.push_back(rc.view());
        rcs.push_back("e"sv);

        if (rcs.size() == 2)
        {
            m.respond("No RCs", true);
            return;
        }

        if (!compressor)
            compressor.emplace();

        // Our output is a dict containing a single key `z` which contains the zstd-compressed bytes
        // of a bt-encoded list of all RCs.

        std::vector<std::byte> response_raw;
        // We don't know the size in advance, so write a dummy value, compress, then fill it in:
        constexpr auto compress_prefix = "d1:z999999999:"sv;
        try
        {
            response_raw = compressor->compress(rcs, zstd::compressor::DEFAULT_LEVEL, compress_prefix);
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Bootstrap request RCs compression failed: {}", e.what());
            m.respond("Compress failed", true);
            return;
        }

        size_t comp_size = response_raw.size() - compress_prefix.size();

#ifndef NDEBUG
        size_t rcs_size = 0;
        for (auto& rc : rcs)
            rcs_size += rc.size();
        log::debug(
            logcat,
            "compressed RC list to {}B ({:.1f}% of raw {}B)",
            comp_size,
            comp_size * 100.0 / rcs_size,
            rcs_size);
#endif

        // Now we need to rewrite the actual `d1:ZNNN:` prefix with the correct NNN for the
        // compressed data:
        std::string actual = "d1:Z{}:"_format(comp_size);
        assert(actual.size() <= compress_prefix.size());
        // Our actual size is almost certainly shorter than the 999999999 value we used, so we skip
        // however many leading bytes as needed to represent the proper final value without needing
        // to shift the compressed data around in the buffer:
        size_t skip = compress_prefix.size() - actual.size();
        std::memcpy(response_raw.data() + skip, actual.data(), actual.size());
        // Response dict terminator:
        response_raw.push_back(std::byte{'e'});

        m.respond(std::span{response_raw.data() + skip, response_raw.size() - skip});
    }

    void Manager::handle_fetch_rcs(
        std::span<const std::byte> body,
        std::function<void(std::string)> respond,
        [[maybe_unused]] bool source_is_relay)
    {
        log::debug(logcat, "Handling FetchRC request...");
        // this handler should not be registered for clients
        assert(router.is_service_node);

        std::unordered_set<RouterID> explicit_ids;

        try
        {
            auto btdc = oxenc::bt_dict_consumer{body};
            for (auto sublist = btdc.require<oxenc::bt_list_consumer>("x"); !sublist.is_finished();)
                explicit_ids.emplace(sublist.consume_span<uint8_t, 32>());
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception handling RC Fetch request: {}", e.what());
            respond(messages::ERROR_RESPONSE);
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

        respond(std::move(btdp).str());
    }

    void Manager::handle_path_fetch_rcs(std::span<const std::byte> body, std::function<void(std::string)> respond)
    {
        handle_fetch_rcs(std::move(body), std::move(respond), false);
    }

    void Manager::handle_path_fetch_router_ids(
        [[maybe_unused]] std::span<const std::byte> body, std::function<void(std::string)> respond)
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
        respond(std::move(btdp).str());
    }

    void Manager::handle_path_resolve_sns(std::span<const std::byte> body, std::function<void(std::string)> respond)
    {
#ifdef LOKINET_EMBEDDED_ONLY
        throw std::logic_error{"This lokinet is not a service node!"};
#else
        log::trace(logcat, "Received request to publish client contact!");

        std::string name_hash;

        try
        {
            name_hash = ResolveSNS::deserialize(oxenc::bt_dict_consumer{body});
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception: {}", e.what());
            return respond(messages::ERROR_RESPONSE);
        }

        assert(router.oxend());
        router.oxend()->lookup_sns_hash(
            name_hash, [respond = std::move(respond)](std::optional<EncryptedSNSRecord> maybe_enc) mutable {
                if (maybe_enc)
                {
                    log::info(logcat, "RPC lookup successfully returned encrypted SNS record!");
                    auto resp = ResolveSNS::serialize_response(*maybe_enc);
                    // FIXME: eventually respond func should take a byte span or something, but
                    //        string was easier for now
                    respond(std::string{reinterpret_cast<const char*>(resp.data()), resp.size()});
                }
                else
                {
                    log::warning(logcat, "RPC lookup could not find SNS registry!");
                    respond(ResolveSNS::NOT_FOUND);
                }
            });
#endif
    }

    void Manager::handle_publish_cc(
        std::span<const std::byte> body, std::function<void(std::string)> respond, bool source_is_relay)
    {
        log::trace(logcat, "Received request to publish client contact!");

        EncryptedClientContact enc;
        std::optional<int> location;

        try
        {
            std::tie(enc, location) = PublishClientContact::deserialize(oxenc::bt_dict_consumer{body});
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception: {}: payload: {}", e.what(), buffer_printer{body});
            return respond(messages::ERROR_RESPONSE);
        }

        if (enc.is_expired())
        {
            log::warning(logcat, "Received expired EncryptedClientContact!");
            return respond(PublishClientContact::EXPIRED);
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
            respond("FIXME!");

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
        auto closest_rids = router.node_db().find_many_closest_to(cc_blind_pk, path::CC_PUBLISH_LOCATIONS + 1);
        if (closest_rids.size() < path::CC_PUBLISH_LOCATIONS)
        {
            respond(messages::serialize_status_response("No RCs available!"));
            return;
        }

        if (!source_is_relay)
        {
            if (!location || *location < 0 || *location >= path::CC_PUBLISH_LOCATIONS)
            {
                log::warning(
                    logcat,
                    "Ignoring ECC publish from a client with {} publish index",
                    location ? "invalid ({})"_format(*location) : "missing");
                respond(messages::serialize_status_response(
                    location ? "INVALID PUBLISH LOCATION" : "MISSING PUBLISH LOCATION"));
                return;
            }

            const auto& rid = closest_rids[*location];

            if (rid == router.id())
            {
                // Special case: we *are* the intended location
                router.contact_db().put_cc(std::move(enc));
                respond(messages::OK_RESPONSE);
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
                [respond = std::move(respond)](quic::message msg) mutable {
                    log::info(
                        logcat,
                        "Relayed PublishClientContact {}! Relaying response...",
                        msg                 ? "SUCCEEDED"
                            : msg.timed_out ? "timed out"
                                            : "failed");
                    log::trace(logcat, "Relayed PublishClientContact response: {}", buffer_printer{msg.body()});
                    respond(std::string{msg.body()});
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
            if (rid == router.id())
            {
                router.contact_db().put_cc(std::move(enc));
                respond(messages::OK_RESPONSE);
                return;
            }

        log::warning(
            logcat, "Ignoring forwarded CC publish: we are not in the top {} publish locations", closest_rids.size());
        respond(messages::ERROR_RESPONSE);
        return;
    }

    void Manager::handle_path_publish_cc(std::span<const std::byte> body, std::function<void(std::string)> respond)
    {
        handle_publish_cc(std::move(body), std::move(respond), false);
    }

    void Manager::handle_find_cc(
        std::span<const std::byte> body, std::function<void(std::string)> respond, bool source_is_relay)
    {
        log::trace(logcat, "Received request to find client contact!");

        PubKey blinded_pubkey;
        try
        {
            blinded_pubkey = FindClientContact::deserialize(oxenc::bt_dict_consumer{body});
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception: {}", e.what());
            return respond(messages::ERROR_RESPONSE);
        }

        auto closest_rids = router.node_db().find_many_closest_to(blinded_pubkey, path::CC_PUBLISH_LOCATIONS);
        if (closest_rids.size() < path::CC_PUBLISH_LOCATIONS)
            return respond(messages::ERROR_RESPONSE);

        // We don't provide the answer ourselves unless we are in the closest-4 set because it's
        // possible we *were* in the closest 4 but then dropped out, but still have a stale record
        // hanging around.
        auto authoritative = std::ranges::count(closest_rids, router.id());
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
                auto resp = FindClientContact::serialize_response(*maybe_cc);
                // FIXME: eventually respond func should take a byte span or something, but
                //        string was easier for now
                return respond(std::string{reinterpret_cast<const char*>(resp.data()), resp.size()});
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
        if (source_is_relay)
        {
            log::critical(
                logcat,
                "Received relayed FindClientContact request (key: {}); could not find locally, relaying "
                "error...",
                blinded_pubkey);
            return respond(FindClientContact::NOT_FOUND);
        }

        auto remaining = std::make_shared<size_t>(closest_rids.size() - authoritative);
        auto hook = [respond = std::move(respond), remaining](quic::message msg) mutable {
            if (*remaining == 0)
                return;  // Already answered by an earlier response

            if (msg)
            {
                *remaining = 0;
                log::info(logcat, "Relayed FindClientContact request SUCCEEDED! Relaying response");
                log::trace(logcat, "Relayed FindClientContact response: {}", buffer_printer{msg.body()});
                respond(std::string{msg.body()});
                return;
            }

            if (--*remaining == 0)
                return;  // This was an error, but there are more responses to come back

            log::warning(logcat, "All FindClientContact requests FAILED! Relaying failure");
            respond(msg.timed_out ? messages::TIMEOUT_RESPONSE : std::string{msg.body()});
        };

        log::debug(logcat, "Relaying FindClientContactMessage (key: {}) to {} peers", blinded_pubkey, *remaining);

        auto forwarded_find_cc = FindClientContact::serialize(blinded_pubkey);
        for (const auto& rid : closest_rids)
        {
            if (rid == router.id())
                continue;
            endpoint.send_command(rid, "find_cc", forwarded_find_cc, hook);
        }
    }

    void Manager::handle_path_find_cc(std::span<const std::byte> body, std::function<void(std::string)> respond)
    {
        handle_find_cc(std::move(body), std::move(respond), false);
    }

    void Manager::handle_path_build(quic::message m, const std::variant<RouterID, quic::ConnectionID>& from)
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

    void Manager::handle_path_control(quic::message m)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        auto body = m.body<std::byte>();
        if (body.size() <= path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD_MAC)
        {
            log::warning(logcat, "Received path control message too small to contain valid data.");
            m.respond(messages::ERROR_RESPONSE, true);
            return;
        }

        HopID hop_id;
        std::vector<std::byte> payload;
        SymmNonce nonce;

        payload.assign(body.begin(), body.end());

        static_assert(
            path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD_MAC == crypto::MAC_SIZE + SymmNonce::SIZE + HopID::SIZE + 1);
        auto [inner_payload, bnonce, bhop, msgtype] = split_span_tail<SymmNonce::SIZE, HopID::SIZE, 1>(payload);
        std::byte type = msgtype[0];
        if (type != std::byte{0x01})
        {
            log::warning(logcat, "Invalid/unknown path_control encrypted message type {}", static_cast<int>(type));
            log::trace(logcat, "Failed path_control payload: {}", buffer_printer{body});
            m.respond(messages::ERROR_RESPONSE, true);
            return;
        }
        nonce.assign(bnonce);
        hop_id.assign(bhop);

        auto hop = router.path_context.get_transit_hop_ptr(hop_id);
        if (not hop)
        {
            log::warning(logcat, "Received path control for unknown path (hop ID: {})", hop_id);
            m.respond(messages::ERROR_RESPONSE, true);
            return;
        }

        nonce ^= hop->xor_nonce;

        // if terminal hop, payload should contain a request (e.g. "sns_resolve"); handle and respond.
        if (hop->terminal_hop)
        {
            auto payload_span = crypto::xchacha20_poly1305_decrypt(inner_payload, hop->shared_secret, nonce);
            auto responder = [hop_weak = std::weak_ptr{hop}, msg = std::move(m), type](std::string response) {
                auto hop = hop_weak.lock();
                if (not hop)
                {
                    log::info(logcat, "Received response to path control message, but no transit hop found; dropping.");
                    return;
                }
                auto& hopid = hop->rxid;
                auto nonce = SymmNonce::make_random();
                response.reserve(response.size() + path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD_MAC);
                response.resize(response.size() + crypto::MAC_SIZE);
                try
                {
                    crypto::xchacha20_poly1305_encrypt(response, hop->shared_secret, nonce);
                }
                catch (const std::exception& e)
                {
                    log::warning(logcat, "Failed encrypting path control message response: {}", e.what());
                    return;
                }
                nonce ^= hop->xor_nonce;
                auto inner_size = response.size();
                response.resize(inner_size + path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD);
                auto resp_span = oxen::quic::reinterpret_span<std::byte>(std::span{response});

                static_assert(sizeof(SymmNonce) == SymmNonce::SIZE);
                static_assert(sizeof(HopID) == HopID::SIZE);

                auto [inner_payload, bnonce, bhop, msgtype] =
                    split_span_tail<SymmNonce::SIZE, HopID::SIZE, 1>(resp_span);
                assert(inner_payload.size() == inner_size);

                nonce.copy_to(bnonce);
                hopid.copy_to(bhop);
                msgtype[0] = type;
                msg.respond(response, false);
            };
            log::debug(logcat, "We are terminal hop for path request: {}", *hop);
            handle_path_request(payload_span, std::move(responder));
            return;
        }

        // intermediate hops chacha the whole payload (MAC included)
        crypto::xchacha20(inner_payload, hop->shared_secret, nonce);

        log::debug(logcat, "We are intermediate hop for path request: {}", *hop);

        const auto [next_target, next_hopid] = hop->next_id(hop_id);

        // We're relaying this message down a path, and we've already done our decryption to the
        // inner_payload so now we just need to replace the nonce and next hop ID in the outer
        // payload before passing it along:
        nonce.copy_to(bnonce);
        next_hopid.copy_to(bhop);

        endpoint.send_command(
            next_target,
            "path_control",
            std::move(payload),
            [hop_weak = std::weak_ptr{hop}, prev_message = std::move(m), type](quic::message response) mutable {
                auto hop = hop_weak.lock();
                if (not hop)
                {
                    log::info(logcat, "Received response to path control message, but no transit hop found; dropping.");
                    return;
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
                {
                    log::warning(logcat, "Path control message returned an error!");
                    prev_message.respond(response.body(), response.is_error());
                }

                std::vector<std::byte> payload;

                auto body = response.body<std::byte>();

                if (body.size() <= path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD)
                {
                    prev_message.respond(messages::ERROR_RESPONSE, true);
                    return;
                }

                payload.assign(body.begin(), body.end());
                auto [inner_payload, bnonce, bhop, msgtype] = split_span_tail<SymmNonce::SIZE, HopID::SIZE, 1>(payload);
                if (msgtype[0] != type)
                {
                    log::warning(
                        logcat,
                        "Path control message response type byte mismatch!  Expected {}, got {}",
                        static_cast<int>(type),
                        static_cast<int>(msgtype[0]));
                    prev_message.respond(messages::ERROR_RESPONSE, true);
                    return;
                }
                HopID recv_hopid;
                SymmNonce nonce;
                recv_hopid.assign(bhop);
                nonce.assign(bnonce);
                if (recv_hopid != hop->txid)
                {
                    log::warning(logcat, "Path control message response type unexpected hop id...");
                    prev_message.respond(messages::ERROR_RESPONSE, true);
                    return;
                }

                crypto::xchacha20(inner_payload, hop->shared_secret, nonce);

                nonce ^= hop->xor_nonce;
                nonce.copy_to(bnonce);
                hop->rxid.copy_to(bhop);

                prev_message.respond(payload, false);
            });
    }

    void Manager::handle_path_session_control(quic::message m)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        auto body = m.body<std::byte>();
        if (body.size() <= path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD_MAC)
        {
            log::warning(logcat, "Received session control message too small to contain valid data.");
            return;
        }
        std::vector<std::byte> payload;
        payload.assign(body.begin(), body.end());
        handle_session_message(std::move(payload), true);
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
    static std::optional<std::tuple<HopID, SymmNonce, std::byte>> extract_path_message_metadata(
        std::vector<std::byte>& message)
    {
        if (message.size() < MIN_PATH_DATA_MESSAGE_SIZE)
        {
            log::warning(logcat, "Dropping invalid too-short path data message");
            return std::nullopt;
        }

        // Deliberately break compilation if data message overhead changes in path without getting
        // updated here as well:
        static_assert(path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD == SymmNonce::SIZE + HopID::SIZE + 1);

        std::optional<std::tuple<HopID, SymmNonce, std::byte>> result;
        auto& [hop_id, nonce, msgtype] = result.emplace();

        // For the detailed structure of this encoding, see description in session/session.cpp
        msgtype = message.back();
        if (msgtype != std::byte{0x01} && msgtype != std::byte{0x02})
        {
            log::warning(logcat, "Received path message of unknown type: {}", std::to_integer<int>(msgtype));
            return std::nullopt;
        }
        message.pop_back();

        hop_id.assign(std::span{message}.last<HopID::SIZE>());
        message.resize(message.size() - HopID::SIZE);

        nonce.assign(std::span{message}.last<SymmNonce::SIZE>());
        message.resize(message.size() - SymmNonce::SIZE);

        return result;
    }

    void Manager::handle_session_message(std::vector<std::byte> message, bool control)
    {
        auto maybe_hop_nonce = extract_path_message_metadata(message);
        if (!maybe_hop_nonce)
            return;
        auto& [hop_id, nonce, msgtype] = *maybe_hop_nonce;

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

            // NB: path switch is a special case, as it comes bundled as 2 messages:
            //     a path switch session control message, and
            //     a session init message
            if (msgtype == path::Path::PATH_SWITCH_MESSAGE_TYPE)
            {
                log::debug(logcat, "client handling path switch/session init message");

                if (message.size() < sizeof(session::session_tag))
                {
                    log::info(logcat, "invalid path switch / session reinit message received");
                    return;
                }
                session_tag tag;
                auto tag_span = std::span{message}.last<sizeof(session_tag)>();
                tag = oxenc::load_big_to_host<session_tag>(tag_span.data());
                message.resize(message.size() - sizeof(session::session_tag));

                oxenc::bt_list_consumer btlc{message};
                auto path_switch = btlc.consume_string_view();
                auto session_init = btlc.consume_string_view();
                btlc.finish();

                if (router.session_endpoint().get_session(tag))
                {
                    log::debug(logcat, "Handling incoming session path switch message at the client end of a path");
                    // to avoid nonce re-use, mutate by xor factor
                    nonce ^= session::switch_xor_factor;
                    std::vector<std::byte> bytes;
                    bytes.resize(path_switch.size());
                    std::memcpy(bytes.data(), path_switch.data(), bytes.size());
                    handle_session_control(std::move(bytes), tag, nonce, path->shared_from_this());
                }
                else
                {
                    log::debug(logcat, "Handling incoming session init message at the client end of a path");
                    std::vector<std::byte> bytes;
                    bytes.resize(session_init.size());
                    std::memcpy(bytes.data(), session_init.data(), bytes.size());
                    router.session_endpoint().handle_session_init(std::move(bytes), path->shared_from_this());
                }
                return;
            }

            // Client-bound session data has no pivot, just [encrypted][sessiontag], so we extract
            // and remove the session tag then give the remainder for be session-decrypted.  The
            // nonce (after the above mutations) also matches the nonce we want to use for the
            // session encryption.
            session_tag tag;
            auto tag_span = std::span{message}.last<sizeof(session_tag)>();
            tag = oxenc::load_big_to_host<session_tag>(tag_span.data());
            message.resize(message.size() - sizeof(session_tag));

            if (tag == 0)  // session init
            {
                return router.session_endpoint().handle_session_init(std::move(message), path->shared_from_this());
            }
            if (control)
            {
                log::trace(logcat, "Handling incoming session control message at the client end of a path");
                handle_session_control(std::move(message), tag, nonce, path->shared_from_this());
            }
            else
            {
                log::trace(logcat, "Handling incoming data message at the client end of a path");
                handle_session_data(std::move(message), tag, nonce);
            }
            return;
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

            auto [payload, bsession_tag, bpivot_id] = split_span_tail(message, sizeof(session_tag), HopID::SIZE);

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

                // NB: path switch is a special case, as it comes bundled as 2 messages:
                //     a path switch session control message, and
                //     a session init message
                if (msgtype == path::Path::PATH_SWITCH_MESSAGE_TYPE)
                {
                    log::debug(logcat, "client handling path switch/session init message");

                    oxenc::bt_list_consumer btlc{payload};
                    auto path_switch = btlc.consume_string_view();
                    auto session_init = btlc.consume_string_view();
                    btlc.finish();

                    session_tag tag;
                    tag = oxenc::load_big_to_host<session_tag>(bsession_tag.data());
                    if (router.session_endpoint().get_session(tag))
                    {
                        log::debug(logcat, "Handling incoming session path switch message at the relay end of a path");
                        // to avoid nonce re-use, mutate by xor factor
                        nonce ^= session::switch_xor_factor;
                        std::vector<std::byte> bytes;
                        bytes.resize(path_switch.size());
                        std::memcpy(bytes.data(), path_switch.data(), bytes.size());
                        handle_session_control(
                            std::move(bytes), tag, nonce, router.path_context.get_transit_hop_ptr(hop_id));
                    }
                    else
                    {
                        log::debug(logcat, "Handling incoming session init message at the relay end of a path");
                        std::vector<std::byte> bytes;
                        bytes.resize(session_init.size());
                        std::memcpy(bytes.data(), session_init.data(), bytes.size());
                        router.session_endpoint().handle_session_init(
                            std::move(bytes), router.path_context.get_transit_hop_ptr(hop_id));
                    }
                    return;
                }

                session_tag tag;
                auto tag_span = bsession_tag.first<sizeof(session_tag)>();
                tag = oxenc::load_big_to_host<session_tag>(tag_span.data());
                message.resize(payload.size());

                if (tag == 0)  // session init
                {
                    // getting shared_ptr here instead of above saves an atomic op on other messages
                    return router.session_endpoint().handle_session_init(
                        std::move(message), router.path_context.get_transit_hop_ptr(hop_id));
                }
                if (control)
                {
                    log::trace(logcat, "Incoming control message is a relay session control message");
                    handle_session_control(
                        std::move(message), tag, nonce, router.path_context.get_transit_hop_ptr(hop_id));
                }
                else
                {
                    log::trace(logcat, "Incoming data message is a relay session data message");
                    handle_session_data(std::move(message), tag, nonce);
                }
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
            auto [session_payload, bnonce, bhop, bmsgtype] = split_span_tail<SymmNonce::SIZE, HopID::SIZE, 1>(message);

            nonce ^= trans_hop->xor_nonce;
            crypto::xchacha20(session_payload, trans_hop->shared_secret, nonce);

            nonce.copy_to(bnonce);
            pivot_id.copy_to(bhop);
            bmsgtype[0] = msgtype;

            log::trace(logcat, "Pivoting message down another path");
            if (control)
                endpoint.send_command(trans_hop->downstream, "session_control"s, std::move(message), nullptr);
            else
                endpoint.send_datagram(trans_hop->downstream, std::move(message));
            return;
        }

        // Case 4: we're an intermediate so we forward it to the next hop
        const auto [next_target, next_hopid] = hop->next_id(hop_id);

        // We chopped off the 0x01, hop_id, and nonce at the top of this function, but now lets put
        // the new ones back on to make it suitable for the next hop.  (We're just resizing a vector
        // down and back up, so there's no reallocation or copying happening by the resizing and
        // little point in trying to avoid it).
        static_assert(path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD == SymmNonce::SIZE + HopID::SIZE + 1);
        message.resize(message.size() + path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD);
        auto [enc_data, bnonce, bhop, bmsgtype] = split_span_tail<SymmNonce::SIZE, HopID::SIZE, 1>(message);
        nonce.copy_to(bnonce);
        next_hopid.copy_to(bhop);
        bmsgtype[0] = msgtype;

        if (control)
            endpoint.send_command(next_target, "session_control"s, std::move(message), nullptr);
        else
            endpoint.send_datagram(next_target, std::move(message));
    }

    void Manager::handle_session_data(std::vector<std::byte>&& payload, const session_tag& tag, const SymmNonce& nonce)
    {
        if (auto session = router.session_endpoint().get_session(tag))
            session->recv_session_data_message(std::move(payload), nonce);
        else
            log::warning(logcat, "Could not find session {} to receive session data message!", tag);
    }

    void Manager::handle_session_control(
        std::vector<std::byte>&& payload,
        const session_tag& tag,
        const SymmNonce& nonce,
        std::variant<std::shared_ptr<path::TransitHop>, std::shared_ptr<path::Path>> source)
    {
        try
        {
            if (auto session = router.session_endpoint().get_session(tag))
                session->recv_session_control_message(std::move(payload), nonce, source);
            else
                log::warning(logcat, "Could not find session {} to receive session control message!", tag);
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Error handling session control message: {}", e.what());
        }
    }

    void Manager::handle_path_request(std::span<const std::byte> payload, std::function<void(std::string)> respond)
    {
        std::string endpoint, body;

        try
        {
            // FIXME: unnecessary copy
            std::tie(endpoint, body) = PATH::CONTROL::deserialize(oxenc::bt_dict_consumer{payload});
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception: {}; Payload: {}", e.what(), buffer_printer{payload});
            return respond(messages::serialize_status_response("ERROR"));
        }

        if (auto it = path_requests.find(endpoint); it != path_requests.end())
        {
            log::debug(logcat, "Received path control request (`{}`); invoking endpoint...", endpoint);
            (this->*(it->second))(oxen::quic::reinterpret_span<std::byte>(std::span{body}), std::move(respond));
        }
        else
            log::warning(logcat, "Received path control request (`{}`), which has no local handler!", endpoint);
    }

    void Manager::handle_path_ping(std::span<const std::byte>, std::function<void(std::string)> respond)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        respond(messages::OK_RESPONSE);
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
