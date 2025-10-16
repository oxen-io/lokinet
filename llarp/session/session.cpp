#include "session.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/handlers/session.hpp>
#include <llarp/handlers/tun.hpp>
#include <llarp/link/endpoint.hpp>
#include <llarp/messages/dht.hpp>
#include <llarp/messages/path.hpp>
#include <llarp/net/policy.hpp>
#include <llarp/path/transit_hop.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/bspan.hpp>
#include <llarp/util/formattable.hpp>
#include <llarp/util/random.hpp>
#include <llarp/util/time.hpp>

#include <nlohmann/json.hpp>
#include <oxen/quic/context.hpp>
#include <oxen/quic/gnutls_crypto.hpp>
#include <oxen/quic/udp.hpp>
#include <oxenc/endian.h>
#include <oxenc/hex.h>

#include <chrono>
#include <limits>
#include <random>
#include <utility>

namespace
{
    using namespace oxenc::literals;
    // GNUTLS Creds tunnel default keys until we implement null-crypto in libquic
    inline constexpr auto TUNNEL_SEED = "0000000000000000000000000000000000000000000000000000000000000000"_hex;
    inline constexpr auto TUNNEL_PUBKEY = "3b6a27bcceb6a42d62a3a8d02a6f0d73653215771de243a63ac048a18b59da29"_hex;
}  // anonymous namespace

namespace llarp::session
{
    namespace quic = oxen::quic;

    static auto logcat = log::Cat("session");
    struct TCPTunnel
    {
        const quic::Address FAKE_QUIC_ADDR{"127.86.75.30"s, 9};
        const quic::Path FAKE_QUIC_PATH{FAKE_QUIC_ADDR, FAKE_QUIC_ADDR};

        std::shared_ptr<quic::GNUTLSCreds> tls_creds = quic::GNUTLSCreds::make_from_ed_keys(TUNNEL_SEED, TUNNEL_PUBKEY);

        std::shared_ptr<quic::Endpoint> quic_ep{nullptr};
        std::shared_ptr<quic::Connection> quic_conn{nullptr};

        // (session initiator) TCPHandle listeners mapped to the destination port they are mapped for
        std::unordered_map<uint16_t, std::shared_ptr<TCPHandle>> tcp_handles;

        // (session remote) QUIC stream ID to TCP connection
        std::vector<std::shared_ptr<TCPConnection>> _tcp_conns;

        Session& session;

        std::shared_ptr<bool> destructor_canary{std::make_shared<bool>(true)};

        ~TCPTunnel() { reset(); }

        // the QUIC endpoint should be fine if the QUIC connection closes, but
        // TCP conns and port mappings will need to be restarted.
        void reset()
        {
            log::critical(logcat, "TCPTunnel::reset()");
            quic_conn.reset();
            _tcp_conns.clear();
            tcp_handles.clear();
            log::critical(logcat, "TCPTunnel::reset() END");
        }

        TCPTunnel(Session& _session) : session(_session)
        {
            quic::opt::manual_routing quic_send{[this](const quic::Path&, std::span<const std::byte> data) {
                session.send_session_data_message(data, traffic_type::TUNNELED_QUIC);
            }};
            quic::connection_established_callback new_conn{[this](quic::Connection& conn) {
                if (quic_conn)
                {
                    log::error(logcat, "Already have connection for QUIC tunnel for session to {}!", session._remote);
                    return;
                }
                log::debug(logcat, "New connection for QUIC tunnel for session to {}!", session._remote);
                quic_conn = conn.shared_from_this();
            }};
            quic::connection_closed_callback conn_closed{
                [this, canary = std::weak_ptr{destructor_canary}](quic::Connection&, uint64_t) {
                    if (!quic_conn)
                    {
                        log::warning(
                            logcat,
                            "Received conn closed, but this session's QUIC tunnel does not seem to have an open "
                            "connection, remote: {}",
                            session._remote);
                        return;
                    }
                    log::debug(logcat, "QUIC TCP tunnel conn to {} closed.", session._remote);

                    // this could fire from quic::Endpoint destructor, at which point
                    // the members `reset` would reset may no longer be valid objects
                    if (canary.lock())
                        reset();
                }};

            auto stream_opened = [this](quic::Stream& stream) {
/*
                stream.set_stream_data_cb([this, prev_byte = std::optional<std::byte>{std::nullopt}](
                                              quic::Stream& stream, std::span<const std::byte> data) mutable {
                    uint16_t dest_port{0};

                    if (data.empty())
                    {
                        log::error(logcat, "QUIC stream data callback with no data!");
                        return;
                    }
                    if (prev_byte)
                    {
                        std::array<std::byte, 2> buf;
                        buf[0] = *prev_byte;
                        buf[1] = data[0];
                        dest_port = oxenc::load_big_to_host<uint16_t>(buf.data());
                        data = data.subspan(1);
                    }
                    else if (data.size() >= 2)
                    {
                        dest_port = oxenc::load_big_to_host<uint16_t>(data.data());
                        data = data.subspan(2);
                    }
                    else
                    {  // only got 1 byte total so far, need 2 for dest port
                        prev_byte = data[0];
                        return;
                    }

                    stream.pause();

                    // FIXME: TCPHandle::connect replaces the stream's data callback.  Perhaps
                    // that should happen here instead.
                    // FIXME: the connection should probably come from tun bind address, if
                    // available, rather than always 127.0.0.1
                    auto tcp_conn = TCPHandle::connect(
                        session._r.loop.get_event_base(), FAKE_QUIC_ADDR, stream.shared_from_this(), dest_port);
                    if (!tcp_conn)
                    {
                        stream.close(11223322);  // TODO: meaningful error code
                        return;
                    }

                    _tcp_conns.push_back(tcp_conn);

                    if (data.size())
                    {
                        // put any remaining stream data on the tcp socket
                        stream.data_callback(stream, data);
                    }

                    stream.enable_watermarks(
                        500'000,
                        [this, tcp_conn](auto&) { tcp_conn->stop_reading(); },
                        50'000,
                        [this, tcp_conn](auto&) { tcp_conn->resume_reading(); });
                });
*/
                return 0;
            };

            quic_ep = quic::Endpoint::endpoint(
                // TODO FIXME: this should probably attach to the network loop rather than the logic loop:
                session._r.loop,
                FAKE_QUIC_ADDR,
                std::move(quic_send),
                std::move(new_conn),
                std::move(conn_closed),
                quic::opt::disable_mtu_discovery{});

            // TODO: only listen if we support inbound tunneled traffic
            quic_ep->listen(tls_creds, std::move(stream_opened));
        }

        void open_connection()
        {
            if (quic_conn)
            {
                log::error(
                    logcat, "Cannot create more than one QUIC connection over TPC tunnel, remote: {}", session._remote);
                return;
            }

            quic_conn = quic_ep->connect(
                quic::RemoteAddress{TUNNEL_PUBKEY, FAKE_QUIC_ADDR},
                tls_creds,
                [this](quic::Connection& conn) {
                    log::debug(logcat, "Outbound QUIC TCP Tunnel connection established to {}", session._remote);
                    if (!quic_conn)
                    {
                        quic_conn = conn.shared_from_this();
                    }
                },  // connection established
                [this, canary = std::weak_ptr{destructor_canary}](quic::Connection&, uint64_t) /* connection closed*/ {
                    // this could fire from quic::Endpoint destructor, at which point
                    // the members referenced below may no longer be valid objects
                    if (!canary.lock())
                        return;
                    if (!quic_conn)
                        log::error(logcat, "QUIC TPC tunnel connection to {} failed!", session._remote);
                    else
                        log::debug(logcat, "QUIC TPC tunnel connection to {} closed.", session._remote);
                    reset();
                });
        }

        uint16_t map_tcp_remote_port(uint16_t dest_port)
        {
            if (!session.is_established())
                return 0;
            if (!quic_conn)
            {
                open_connection();
            }

            auto _handle = TCPHandle::make_server(
                // TODO FIXME: this should probably attach to the network loop rather than the logic loop:
                session._r.loop,
                [this, dest_port](struct bufferevent* _bev, evutil_socket_t _fd) -> TCPConnection* {
                    auto s =
                        quic_conn->open_stream<quic::Stream>([_bev](quic::Stream& s, std::span<const std::byte> data) {
                            auto rv = bufferevent_write(_bev, data.data(), data.size());

                            log::debug(
                                logcat,
                                "Stream (id:{}) {} {}B to TCP buffer",
                                s.stream_id(),
                                rv < 0 ? "failed to write" : "successfully wrote",
                                data.size());
                        });
                    if (!s)
                    {
                        log::error(logcat, "Failed to open stream for TCP tunnel...");
                        return nullptr;
                    }
                    std::string p;
                    p.resize(2);
                    oxenc::write_host_as_big(dest_port, p.data());
                    s->send(std::move(p));

                    auto tcp_conn = std::make_shared<TCPConnection>(_bev, _fd, std::move(s));

                    auto* ptr = tcp_conn.get();
                    _tcp_conns.push_back(std::move(tcp_conn));

                    return ptr;
                });

            auto bound_port = _handle->port();
            if (bound_port == 0)
            {
                log::error(logcat, "Failed to bind TCP port for tunneled session.");
                return 0;
            }

            log::debug(logcat, "Bound TCP tunneled session, dest_port: {}, local_port: {}", dest_port, bound_port);
            tcp_handles.emplace(dest_port, std::move(_handle));
            return bound_port;
        }
    };

    void InboundSession::init(std::vector<std::byte>&& request)
    {
        oxenc::bt_dict_consumer outer_btdc{request};
        PubKey eph_pubkey;
        SymmNonce dh_nonce;
        eph_pubkey.assign(outer_btdc.require_span<std::byte, PubKey::SIZE>("k"));
        dh_nonce.assign(outer_btdc.require_span<std::byte, SymmNonce::SIZE>("n"));

        // this is a bit ugly, but bt_dict_consumer only gives const spans/views, and
        // it is safe here to have non-const and avoid a copy.
        auto inner_payload_const = outer_btdc.require_span<std::byte>("x");
        std::span<std::byte> inner_payload{
            const_cast<std::byte*>(inner_payload_const.data()), inner_payload_const.size()};
        outer_btdc.finish();

        crypto::dh_server(_shared_secret, eph_pubkey, _r.secret_key(), dh_nonce);
        auto decrypted = crypto::xchacha20_poly1305_decrypt(inner_payload, _shared_secret, dh_nonce);

        oxenc::bt_dict_consumer inner_btdc{decrypted};
        RouterID remote_rid;
        remote_rid.assign(inner_btdc.require_span<std::byte, RouterID::SIZE>("i"));
        _remote = {remote_rid, true};
        _remote_pivot_txid.assign(inner_btdc.require_span<std::byte, HopID::SIZE>("p"));
        _outbound_tag = inner_btdc.require<session_tag>("t");

        inner_btdc.require_signature("~", [remote_rid](std::span<const std::byte> msg, std::span<const std::byte> sig) {
            if (sig.size() != SIGSIZE)
                throw std::runtime_error{fmt::format("Invalid signature: not {} bytes", SIGSIZE)};

            if (not crypto::verify(remote_rid, msg, sig.first<SIGSIZE>()))
                throw std::runtime_error{"Failed to verify session_init identity signature"};
        });
        inner_btdc.finish();
        _inbound_tag = _parent.next_tag();
    }

    Session::Session(
        Router& r, handlers::SessionEndpoint& parent, const NetworkAddress& remote, session_tag inbound_tag)
        : _r{r},
          _parent{parent},
          _inbound_tag{inbound_tag},
          _remote{remote},
          is_outbound{true},
          is_relay_session{_remote.relay()}
    {
        // Maybe we should make this on demand rather than on construction?
        tcp_tunnel = std::make_unique<TCPTunnel>(*this);
    }

    Session::Session(Router& r, handlers::SessionEndpoint& parent)
        : _r{r},
          _parent{parent},
          _is_established{true},  // Inbound sessions are established from construction
          is_outbound{false},
          is_relay_session{_r.is_service_node}
    {
        // Maybe we should make this on demand rather than on construction?
        tcp_tunnel = std::make_unique<TCPTunnel>(*this);
    }

    Session::~Session()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        close(false);
    }

    void Session::update_active() { last_activity = llarp::time_now_ms(); }

    bool Session::send_session_control_message(std::string_view method, std::span<const std::byte> body)
    {
        if (!_is_established)
        {
            log::warning(logcat, "Session not yet established: should not send control messages yet.");
            return false;
        }
        if (_dead_path)
        {
            log::warning(logcat, "Dropping session control message: session has no current path");
            return false;
        }
        auto inner_body = PATH::CONTROL::serialize(method, body);

        send_session_data_message(std::move(inner_body), 0, true);

        return true;
    }

    void Session::recv_session_control_message(
        std::vector<std::byte>&& message,
        const SymmNonce& nonce,
        [[maybe_unused]] std::variant<std::shared_ptr<path::TransitHop>, std::shared_ptr<path::Path>> source)
    {
        update_active();
        auto decrypted = crypto::xchacha20_poly1305_decrypt(message, _shared_secret, nonce);
        if (decrypted.size() == 0)
        {
            log::warning(logcat, "Received unauthenticated session message on session from {}", _remote);
            return;
        }
        auto btdc = oxenc::bt_dict_consumer{decrypted};

        auto method = btdc.require<std::string_view>("e"sv);
        auto params = btdc.require<std::span<const std::byte>>("p"sv);
        log::debug(logcat, "Received session control message for {} of type {}", _remote, method);

        if (method == "session_accept"sv)
            handle_session_accept(params);
        else if (method == "session_close")
            _parent.close_session(_inbound_tag, false);
        else if (method == "publish_cc"sv)
            handle_client_contact(params);
        else if (method == "path_switch"sv)
        {
            if (is_outbound)
            {
                log::warning(logcat, "Received path switch on outbound path, dropping.");
                return;
            }
            auto inner_btdc = oxenc::bt_dict_consumer{params};
            auto hop_span = inner_btdc.require_span<std::byte, HopID::SIZE>("p"sv);
            HopID hopid;
            hopid.assign(hop_span);
            inner_btdc.finish();
            if (is_relay_session)
            {
                auto p = std::get<std::shared_ptr<path::TransitHop>>(std::move(source));
                if (hopid != p->rxid)
                {
                    log::warning(logcat, "Received relay session path switch, hopid mismatch with receiving path.");
                    return;
                }
                static_cast<InboundRelaySession*>(this)->handle_path_switch(std::move(hopid), std::move(p));
            }
            else
            {
                auto p = std::get<std::shared_ptr<path::Path>>(std::move(source));
                if (hopid == p->terminal_hopid())
                {
                    log::warning(logcat, "Received client session path switch, hopid collides with receiving path.");
                    return;
                }
                static_cast<InboundClientSession*>(this)->handle_path_switch(std::move(hopid), std::move(p));
            }
        }
    }

    void InboundClientSession::handle_path_switch(HopID pivot, std::shared_ptr<path::Path> path)
    {
        log::debug(
            logcat,
            "Session with {} switching to path {} with pivot hopid {}",
            _remote.router_id(),
            *path,
            pivot.to_view());
        _current_path = std::move(path);
        _dead_path = !_current_path;
        _remote_pivot_txid = std::move(pivot);
    }

    void InboundRelaySession::handle_path_switch(HopID pivot, std::shared_ptr<path::TransitHop> thop)
    {
        log::debug(logcat, "Session with {} switching to transit hop with pivot hopid {}", _remote.router_id(), pivot);
        _current_thop = std::move(thop);
        _dead_path = !_current_thop;
        _remote_pivot_txid = std::move(pivot);
    }

    void Session::send_session_data_message(std::span<const std::byte> data, net::IPProtocol proto)
    {
        uint8_t type;
        if (proto == net::IPProtocol::UDP)
            type = traffic_type::UDP;
        else if (proto == net::IPProtocol::TCP)
            type = traffic_type::TCP;
        else
            type = traffic_type::RAW;

        return send_session_data_message(data, type);
    }

    // TODO FIXME: we could make this take a vector&& as input, and then provide a
    // SESSION_DATA_MESSAGE constant that callers can use to reserve the needed extra storage before
    // moving the vector into here.
    void Session::send_session_data_message(std::span<const std::byte> data, uint8_t type, bool control, bool init)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (!_is_established and !init)
        {
            log::debug(logcat, "Session not yet established: queuing packet for delayed delivery");
            queue_data_message(data, type);
            return;
        }

        if (_dead_path)
        {
            log::warning(logcat, "Dropping session data message: session has no current path");
            return;
        }

        // We use a single nonce for session + path encryption, but noting that path mutations
        // are applied to reduce traceability.
        //
        // So, for instance, if we send the data message M from client A to client B
        // via pivot P along these aligned paths:
        //
        // A -> X -> Y -> Z -> P <- W <- B
        //
        // then we generate a random nonce N, and encrypt the session message with it.  We then
        // onion the X-Y-Z-P path message, starting with the original N for the pivot, using nonce
        // (N ^ Pm) for hop Z, (N ^ Pm ^ Zm) for Y, and (N ^ Pm ^ Zm ^ Ym) for X.  The nonce that we
        // *send* to X is thus N ^ Pm ^ Zm ^ Ym ^ Xm, so that each hop mutates the nonce with its
        // own xor_nonce to get the decryption nonce it should use.
        //
        // (All of this nonce mutation is not for cryptographic security, but rather simply
        // obfuscates packets somewhat, making it harder to link packets across different hops).
        //
        // The pivot (assuming this is a client-to-client data message) reuses this nonce again
        // down the aligned path, each one mutating it with the xor nonce; the final client
        // then undoes all of the far-side nonce mutations to arrive back at N, which it then
        // uses to also decrypt the *session* level encryption.
        auto nonce = SymmNonce::make_random();

        // As this packet is what carries IP data, we want to make it as small as possible, and thus
        // don't use bt-encoding here.  We also build this "backwards" by putting the parts
        // eliminated first at the end, so that a receiver can drop them off the back of a vector
        // without needing to shift the data.
        //
        // Thus we encode values packed together like this:
        //
        // 1. Encrypted(PAYLOAD + TYPE BYTE)
        // 2. Session tag
        // 3. Pivot ID (sometimes omitted)
        //
        // The PivotID instructs the pivot which aligned path to forward it down, and also
        // identifies inbound relay session data messages: pivot ID == incoming hop ID means the
        // message is a relay session data message.  The pivot ID is omitted for session data that
        // ends at a client (i.e. post-pivot path data, and relay->client data on a relay session).
        //
        // All of the above make up the path's data message payload; when we deliver this down the
        // path to the pivot, it will get encrypted repeatedly for each relay on the path (starting
        // at the pivot), and a path nonce will be generated.  Thus in terms of data down a path we
        // get:
        //
        // [{N}-onion([1][2][3])][N_mutated_nonce][hopid][0x01]
        //
        // where each hop down the path reads the hopid and uses this to get the TransitHop (which
        // it has stored locally during path building), indicating the next hop, shared secret, and
        // nonce mutator.  It uses this to mutate the nonce, and then mutates the payload to decrypt
        // one layer of onion encryption.
        //
        // If that is an intermediate hop (which will be identifiable via the TransitHop info) then
        // it sends a payload of the exact same size to the next hop, but where the onioned data has
        // one onion layer removed, the nonce is replaced with the mutated nonce, and the hopid is
        // replaced with the next hopid as specified during the path build.
        //
        // [{N-1}-onion([1][2][3])][N-1_mutated_nonce][next_hopid][0x01]
        //
        // (Note the 0x01 suffix byte above is essentially a datagram versioning byte indicating
        // that this is a data message, and currently is always 0x01; future versions reserve other
        // values for other potential future uses of the quic datagram channel).
        //
        // If, on the other hand, this payload arrives at the path terminus then the mutated payload
        // will have removed the final layer of onioning and so the payload will have been mutated
        // back to the plaintext [1-3] values listed above.  The pivot then reads the pivot ID,
        // which it uses to look up the aligned path that the the data should be sent along (or to
        // itself, if equal to the incoming hopid).
        //
        // If the pivot id indicates an aligned path (i.e. indicates that this is a pivot) then it
        // uses the pivot id to determine the aligned path's next hop, discards the pivotid from the
        // inner payload, onions it and then feeds the data down the aligned path:
        //
        // [{1}-onion([1][2])][nonce][hopid][0x01]
        //
        // each hop *adding* a layer of onion encryption to the payload and mutating the nonce by
        // its xor_nonce.  ("Decryption" and "encryption" here are more or less just conceptual, as
        // both are really just referring to one application of xchacha20).
        //
        // Finally this arrives at the remote client after the M hops, as:
        //
        // [{M}-onion([1][2])][M_mutated_nonce][final_hopid][0x01]
        //
        // The client then decrypts the *path* data message by forward-applying nonce mutation and
        // onioning for the M nodes in the path, which when leaves a fully decrypted path payload
        // consisting of the original items 1-2 described above ([3] was thrown away at the pivot).
        // [0x01] at this point is also discarded (0x01 has done its job).
        //
        // The client then looks up the session by the session tag [2] (discarding if not found).
        // This then allows it to apply session decryption by reusing the path nonce as the session
        // nonce, and using the shared secret from the looked up session.  Once decrypted, it also
        // drops the session tag off the end of the payload.
        //
        // This then leaves it with a decrypted PAYLOAD + TYPE BYTE, and this can then be dealt with
        // as an IP packet.
        //
        // For a session message *to a relay* the first half of the above is similar, but the pivot
        // ID == incoming hopid allows the terminal to realize that it is a session target rather
        // than a pivot.  Thus instead of starting the encryption down an aligned path, it instead
        // uses the session tag to look up the InboundRelaySession and then decrypts the session
        // message just like a client would (again reusing the path nonce).
        //
        // Session messages *from a relay* to a client are similar, the main difference being that
        // there is never a PivotID [3]: payload omits it from the beginning, and so ends up more
        // closely resembling path data down the "back" path of two aligned paths.
        const bool relay_session_return = !is_outbound && is_relay_session;

        auto tag = init ? 0 : _outbound_tag;

        // control messages do not append a "type" byte like datagrams
        // session init messages are already encrypted and encoded, so no mac here
        size_t chacha_size_with_mac = data.size() + (control ? 0 : 1) + (init ? 0 : crypto::MAC_SIZE);
        std::vector<std::byte> everything;
        auto target_size = chacha_size_with_mac + sizeof(tag) + (relay_session_return ? 0 : _remote_pivot_txid.size());
        everything.reserve(target_size + path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD);
        everything.resize(target_size);
        auto [ciphertext, tag_span, pivot] = split_span(everything, chacha_size_with_mac, sizeof(tag));
        assert(pivot.size() == (relay_session_return ? 0 : _remote_pivot_txid.size()));

        std::memcpy(ciphertext.data(), data.data(), data.size());
        if (!control)
            ciphertext[data.size()] = static_cast<std::byte>(type);
        oxenc::write_host_as_big(tag, tag_span.data());
        if (!relay_session_return)
            std::memcpy(pivot.data(), _remote_pivot_txid.data(), pivot.size());

        if (!init)
            crypto::xchacha20_poly1305_encrypt(ciphertext, _shared_secret, nonce);

        if (control)
            send_path_control_message(std::move(everything), std::move(nonce));
        else
            send_path_data_message(std::move(everything), std::move(nonce));
    }

    void OutboundSession::queue_data_message(std::span<const std::byte> data, uint8_t type)
    {
        update_active();
        if (!pre_establish_data_queue)
            pre_establish_data_queue.emplace();
        else
            while (pre_establish_data_queue->size() >= MAX_QUEUED_PACKETS)
                pre_establish_data_queue->pop_front();

        auto& item = pre_establish_data_queue->emplace_back();
        item.resize(data.size() + 1);
        std::memcpy(item.data(), data.data(), data.size());
        item.back() = static_cast<std::byte>(type);
    }

    void Session::recv_session_data_message(std::vector<std::byte> data, const SymmNonce& nonce)
    {
        update_active();
        if (data.empty())
        {
            log::error(logcat, "received empty session data message!");
            return;
        }

        // FIXME: maybe this decrypt should return the size of the now-cleartext part of the
        //        buffer instead of a span?
        auto dspan = crypto::xchacha20_poly1305_decrypt(data, _shared_secret, nonce);
        data.resize(dspan.size());

        uint8_t dgram_type = std::to_integer<uint8_t>(data.back());
        data.pop_back();
        if (!traffic_type::is_valid(dgram_type))
        {
            log::warning(logcat, "dropping session data message with unknown traffic type {}", dgram_type);
            return;
        }

        bool is_udp = dgram_type == traffic_type::UDP;
        bool is_tunneled = dgram_type == traffic_type::TUNNELED_QUIC;

        if (_r.embedded())
        {
            if (is_udp)
            {
                handle_udp_from_remote(IPPacket{std::move(data)});
            }
            else if (!is_tunneled)
            {
                log::warning(logcat, "Received non-UDP, non-tunneled datagram on embedded client, dropping!");
            }
            else
                tcp_tunnel->quic_ep->manually_receive_packet(
                    oxen::quic::Packet{tcp_tunnel->FAKE_QUIC_PATH, std::move(data)});
            return;
        }

        // Otherwise we're not embedded; if the other side also isn't then this is just a raw IP
        // packet to handle via the tun endpoint, and the same for UDP packets from embedded
        // remotes (which also send raw UDP packets):
        if (dgram_type == traffic_type::TUNNELED_QUIC)
            tcp_tunnel->quic_ep->manually_receive_packet(
                oxen::quic::Packet{tcp_tunnel->FAKE_QUIC_PATH, std::move(data)});
        else
            _r.tun_endpoint()->handle_inbound_packet(IPPacket{std::move(data)}, dgram_type, _remote);
    }

    void Session::publish_client_contact(const EncryptedClientContact& ecc)
    {
        auto payload_sv = ecc.bt_payload();
        auto payload{oxen::quic::reinterpret_span<const std::byte>(payload_sv)};
        send_session_control_message("publish_cc", payload);
    }

    void Session::handle_client_contact(std::span<const std::byte>)
    {
        log::warning(logcat, "Received client contact, but not OutboundClientSession.");
    }

    void OutboundClientSession::handle_client_contact(std::span<const std::byte> payload)
    {
        auto ecc = EncryptedClientContact{payload};
        if (auto cc = ecc.decrypt(_remote.router_id()); cc)
        {
            log::debug(logcat, "Session with {} received valid new client contact, updating.", _remote.router_id());
            update_intros(*cc);
        }
        else
            log::warning(logcat, "Session with {} received invalid new client contact!", _remote.router_id());
    }

    void Session::handle_udp_from_remote(IPPacket&& pkt)
    {
        if (!pkt.is_ip() || pkt.protocol() != net::IPProtocol::UDP)
        {
            log::debug(logcat, "Dropping unsupported non-IPv4/v6 UDP packet");
            return;
        }
        auto source_port = pkt.source_port();
        if (!source_port)
        {
            log::debug(logcat, "Dropping malformed UDP packet: {}", pkt.info_line());
            return;
        }
        log::trace(logcat, "incoming udp packet from remote port {}", *source_port);
        auto itr = udp_handles.find(*source_port);
        if (itr == udp_handles.end())
        {
            log::debug(logcat, "Received udp datagram from unknown source port {}", *source_port);
            return;
        }
        auto& socket = itr->second;
        auto dest_port = *pkt.dest_port();
        log::trace(logcat, "incoming udp packet for pseudo port {}", dest_port);
        if (!udp_remote_ports.contains(dest_port))
        {
            log::warning(logcat, "Received UDP packet destined for an unmapped port ({})", dest_port);
            return;
        }
        dest_port = udp_remote_ports[dest_port];
        log::trace(logcat, "pseudo port maps to client port {}", dest_port);

        auto payload = pkt.udp_data();
        if (payload.empty())
        {
            log::warning(logcat, "Received invalid udp datagram");
            return;
        }
        quic::Address dest = socket->address();
        dest.set_port(dest_port);
        const size_t bufsize = payload.size();
        uint8_t ecn = 0;  // FIXME: do we have any way to obtain this?
        size_t n_pkts = 1;
        auto [ior, sent] = socket->send(quic::Path{socket->address(), dest}, payload.data(), &bufsize, ecn, n_pkts);
        log::trace(
            logcat, "UDP from remote -> socket send to local returned {} (ec={})", ior.success(), ior.error_code);
    }

    uint16_t Session::setup_udp_mapping(uint16_t dest_port)
    {
        if (auto itr = udp_handles.find(dest_port); itr != udp_handles.end())
        {
            auto mapped_port = itr->second->address().port();
            log::debug(logcat, "Returning existing mapped port ({}) for dest port {}", mapped_port, dest_port);
            return mapped_port;
        }
        quic::Address src{"127.0.0.1"s, 0};
        quic::Address dest{"127.0.0.1"s, dest_port};
        auto udp_handle = std::make_unique<quic::UDPSocket>(
            _r.loop.get_event_base(), src, /*gso=*/false, [this, dest = std::move(dest)](quic::Packet&& pkt) {
                auto client_port = pkt.path.remote.port();
                if (!udp_client_ports.contains(client_port))
                {
                    log::debug(logcat, "Adding client port {} to mapping for remote port {}", client_port, dest.port());
                    uint16_t new_port = next_udp_client_port;
                    auto start_port = new_port;
                    while (udp_remote_ports.contains(new_port))
                    {
                        new_port++;
                        if (new_port < 1024)
                            new_port = 1024;
                        if (start_port == new_port)
                            throw std::runtime_error{"Ran out of pseudo-udp ports to use"};
                    }
                    udp_client_ports[client_port] = new_port;
                    udp_remote_ports[new_port] = client_port;
                    log::trace(logcat, "pseudo client port {} for real client port {}", new_port, client_port);
                    client_port = new_port;
                }
                else
                    client_port = udp_client_ports[client_port];

                // ip doesn't matter here, but give remote the source port so we receive responses
                // as destined for that port and know where to send them
                auto src = pkt.path.remote;
                src.set_port(client_port);
                auto payload = pkt.data();
                auto packet = IPPacket::make_udp_packet(src, dest, payload);
                send_session_data_message(packet, traffic_type::UDP);
            });
        auto bound_port = udp_handle->address().port();
        udp_handles[dest_port] = std::move(udp_handle);

        return bound_port;
    }

    uint16_t Session::map_tcp_remote_port(uint16_t dest_port) { return tcp_tunnel->map_tcp_remote_port(dest_port); }

    bool Session::is_established() const { return _is_established && !_is_closed; }

    void Session::close(bool send_close)
    {
        if (_is_closed)
            return;

        _is_closed = true;
        log::debug(logcat, "Session to remote ({}) closed!", _remote);
        if (send_close)
        {
            log::debug(logcat, "Dispatching close session message...");
            send_session_control_message("session_close", {});
        }
    }

    bool Session::is_expired(std::chrono::milliseconds now) const { return now - last_activity > SESSION_TIMEOUT; }

    std::string OutboundSession::to_string() const
    {
        return "OSession:[{}{} | {}]"_format(
            _is_closed            ? "closing"
                : _is_established ? "active"
                                  : "pending",
            is_exit_capable ? ",exit-capable" : "",
            (_current_path && !_current_path->is_dead) ? fmt::to_string(*_current_path) : "<NO-PATH>");
    }
    std::string InboundClientSession::to_string() const
    {
        return "ISession:[{}{} | {}]"_format(
            _is_closed            ? "closing"
                : _is_established ? "active"
                                  : "pending",
            is_exit_capable ? ",exit-capable" : "",
            (_current_path && !_current_path->is_dead) ? fmt::to_string(*_current_path) : "<NO-PATH>");
    }
    std::string InboundRelaySession::to_string() const
    {
        return "ISession:[{}{} | {}]"_format(
            _is_closed            ? "closing"
                : _is_established ? "active"
                                  : "pending",
            is_exit_capable ? ",exit-capable" : "",
            (_current_thop && !_current_thop->is_dead) ? fmt::to_string(*_current_thop) : "<NO-T-HOP>");
    }

    OutboundSession::OutboundSession(
        const NetworkAddress& remote,
        handlers::SessionEndpoint& parent,
        int num_hops,
        session_tag inbound_tag,
        std::function<void(OutboundSession& session)> on_est,
        std::optional<std::chrono::milliseconds> est_timeout)
        : PathHandler{parent.router, parent.router.config().paths.outbound_paths, num_hops},
          Session{router, parent, remote, inbound_tag}
    {
        if (on_est)
            on_established(std::move(on_est), est_timeout);
        // TODO: kick off path builds immediately
    }

    void OutboundSession::fire_waiting(std::chrono::milliseconds now)
    {
        // If we're established then we can immediately fire everything in the queue, otherwise we
        // fire callbacks that have reached their timer (to signal a non-established timeout).
        const bool est = is_established();
        while (!_on_established.empty() && (est || _on_established.top().first <= now))
        {
            _on_established.top().second(*this);
            _on_established.pop();
        }
    }

    void OutboundSession::on_established(
        std::function<void(OutboundSession&)> callback, std::optional<std::chrono::milliseconds> timeout)
    {
        _on_established.emplace(
            llarp::time_now_ms() + timeout.value_or(_r.config().paths.build_timeout), std::move(callback));
    }

    void OutboundSession::tick(std::chrono::milliseconds now)
    {
        if (_is_closed)
            return;
        if (is_expired(now))
        {
            close(false);  // don't send close message -- if we expired they already did for sure
            for (auto& p : active_paths())
                drop_path(p);
            return;
        }
        close_old_paths(now);
        path::PathHandler::tick(now);
        fire_waiting(now);
    }

    template <typename T>
    bool check_dead(std::shared_ptr<T>& path_like, Session& s)
    {
        if (!path_like || path_like->is_dead)
        {
            s._dead_path = true;
            if (path_like)
                path_like.reset();
            return true;
        }
        return false;
    }

    static void send_path_data_impl(
        std::shared_ptr<path::Path>& path, Session& s, std::vector<std::byte>&& data, SymmNonce&& nonce)
    {
        if (check_dead(path, s))
        {
            log::debug(logcat, "Unable to send session data message: no current path");
            return;
        }
        if (!path->is_established())
        {
            // TODO FIXME: queue traffic?  (Perhaps only if `is_outbound` and we have no path?)
            log::debug(logcat, "Unable to send session data message: our current path is not yet established");
            return;
        }

        path->send_path_data_message(std::move(data), std::move(nonce));
    }

    static void send_path_control_impl(
        std::shared_ptr<path::Path>& path, Session& s, std::vector<std::byte>&& data, SymmNonce&& nonce)
    {
        if (check_dead(path, s))
        {
            log::debug(logcat, "Unable to send session control message: no current path");
            return;
        }
        if (!path->is_established())
        {
            log::debug(logcat, "Unable to send session control message: our current path is not yet established");
            return;
        }

        path->send_session_control_message(std::move(data), std::move(nonce));
    }

    void OutboundSession::send_path_data_message(std::vector<std::byte>&& data, SymmNonce&& nonce)
    {
        update_active();
        send_path_data_impl(_current_path, *this, std::move(data), std::move(nonce));
    }
    void InboundClientSession::send_path_data_message(std::vector<std::byte>&& data, SymmNonce&& nonce)
    {
        update_active();
        send_path_data_impl(_current_path, *this, std::move(data), std::move(nonce));
    }

    void OutboundSession::send_path_control_message(std::vector<std::byte>&& data, SymmNonce&& nonce)
    {
        update_active();
        send_path_control_impl(_current_path, *this, std::move(data), std::move(nonce));
    }
    void InboundClientSession::send_path_control_message(std::vector<std::byte>&& data, SymmNonce&& nonce)
    {
        update_active();
        send_path_control_impl(_current_path, *this, std::move(data), std::move(nonce));
    }

    void OutboundSession::close_old_paths(std::chrono::milliseconds now)
    {
        // cf. select_new_current
        //
        // When selecting a new path, we select from all active paths that meet the
        // [paths]:acceptable-expiry threshold, but if we don't find any, select from any that
        // satisfy [paths]:min-expiry, and so we replicate that logic here so that we don't close
        // any paths that select_new_current could choose if it was called right now.

        std::vector<std::pair<path::Path*, std::chrono::milliseconds>> close_me, maybe_close;
        bool found_acceptable_exp = false;
        for (auto& path : active_paths())
        {
            if (_current_path.get() == &path)
                continue;  // never close the current path

            auto expires_in = path.expires_in(now);
            if (expires_in >= router.config().paths.acceptable_expiry)
                found_acceptable_exp = true;

            else if (expires_in < router.config().paths.min_expiry)
                close_me.emplace_back(&path, expires_in);

            // Otherwise this is between min and acceptable, and so is a "maybe": we'll close it
            // only if there are some paths above acceptable, but not if all paths are in-between.
            else if (not found_acceptable_exp)
                maybe_close.emplace_back(&path, expires_in);
        }

        int drops = 0;
        for (auto [path, in] : close_me)
        {
            log::debug(logcat, "Dropping {} unused path {} with imminent expiry (in {})", *this, *path, in);
            drop_path(*path);
            drops++;
        }

        if (found_acceptable_exp)
        {
            for (auto [path, in] : maybe_close)
            {
                log::debug(
                    logcat,
                    "Dropping {} unused path {} because it expires relative soon (in {})"
                    " and we have preferable newer paths",
                    *this,
                    *path,
                    in);
                drop_path(*path);
                drops++;
            }
        }
        if (drops)
            log::debug(logcat, "{} dropped {} paths; have {} remaining", *this, drops, num_paths(now));
        else
            log::trace(logcat, "{} found no close-to-expiry paths to drop", *this);
    }

    OutboundRelaySession::OutboundRelaySession(
        const NetworkAddress& remote,
        handlers::SessionEndpoint& parent,
        session_tag inbound_tag,
        std::function<void(OutboundSession& session)> on_est,
        std::optional<std::chrono::milliseconds> on_est_timeout)
        : OutboundSession{
              remote, parent, parent.router.config().paths.relay_hops(), inbound_tag, std::move(on_est), on_est_timeout}
    {
        _parent.lookup_relay_contact(_remote.router_id(), [this](std::optional<llarp::RelayContact> rc) mutable {
            if (rc)
            {
                log::debug(logcat, "Relay contact for {} found: {}", _remote, *rc);
                // Tick ourself to start building paths without waiting for the next scheduled tick
                tick(llarp::time_now_ms());
            }
            else
            {
                log::debug(logcat, "RC lookup failed for {}", _remote);
                // TODO FIXME: should we close the session?  Retry the lookup?  Start responding
                // with ICMP unreachables?
            }
        });
    }

    void OutboundSession::select_new_current_impl(
        std::vector<std::pair<path::Path*, HopID>>&& good, std::vector<std::pair<path::Path*, HopID>>&& fallback)
    {
        if (good.empty())
            good = std::move(fallback);

        if (good.empty())
        {
            log::warning(logcat, "Unable to select new path to {}: no acceptable active paths", _remote);
            _current_path.reset();
            _dead_path = true;
            return;
        }

        auto& [chosen, hopid] = good[std::uniform_int_distribution<size_t>{0, good.size() - 1}(llarp::csrng)];
        switch_path(*chosen, hopid);
    }

    void OutboundRelaySession::select_new_current()
    {
        // New path selection:
        //
        // Go look at all our current active paths that expire at least [paths]:acceptable-expiry
        // from now, and choose one of them at random.
        //
        // If we can't find any suitable one, fallback to a random selection from any paths within
        // [paths]:min-expiry.
        //
        // If we don't have any of those, either, then we fail and disable the current path; when a
        // path build succeeds it will see that we have no current path and switch to it.

        auto now = llarp::time_now_ms();
        std::vector<std::pair<path::Path*, HopID>> good, fallback;
        for (auto& path : active_paths())
        {
            auto expires_in = path.expires_in(now);
            if (expires_in < router.config().paths.min_expiry)
                continue;

            auto& container = expires_in >= router.config().paths.acceptable_expiry ? good : fallback;
            // path-to-relay: the "pivot" hopid equal to the incoming path id indicates path to
            // relay, i.e. no pivoting:
            container.emplace_back(&path, path.terminal_hopid());
        }

        select_new_current_impl(std::move(good), std::move(fallback));
    }

    void OutboundRelaySession::update_paths(std::chrono::milliseconds /*now*/)
    {
        int needed = _target_paths - num_paths();
        if (needed <= 0)
            return;
        log::debug(
            logcat,
            "OutboundRelaySession building {} paths to remote {} to reach target path count {}",
            needed,
            _remote,
            _target_paths);

        int count = 0;
        while (count < needed && build_path_to_remote(_remote.router_id()))
            count++;

        if (count == needed)
            log::debug(logcat, "SessionEndpoint successfully initiated {} path-builds", needed);
        else
            log::warning(logcat, "SessionEndpoint only initiated {} path-builds (wanted {})", count, needed);
    }

    OutboundClientSession::OutboundClientSession(
        const NetworkAddress& remote,
        handlers::SessionEndpoint& parent,
        session_tag inbound_tag,
        std::function<void(OutboundSession& session)> on_est,
        std::optional<std::chrono::milliseconds> timeout)
        : OutboundSession{
              remote, parent, parent.router.config().paths.client_hops, inbound_tag, std::move(on_est), timeout}
    {
        assert(!is_relay_session);

        refresh_intros();

        log::debug(logcat, "Outbound session to {} initiated", _remote);
    }

    void OutboundClientSession::refresh_intros()
    {
        log::debug(logcat, "Initiating intro lookup for {}", _remote);
        _parent.lookup_client_intro(
            _remote.router_id(), [this, alive = canary()](std::optional<ClientContact> cc) mutable {
                if (!alive.lock())
                    return;
                if (cc)
                {
                    log::debug(logcat, "Session initiation returned client contact: {}", *cc);
                    update_intros(*cc);
                }
                else
                    log::warning(logcat, "Failed to lookup intros for {}", _remote);
            });
    }

    void OutboundClientSession::update_intros(const ClientContact& cc)
    {
        log::debug(logcat, "Update session {} intros from client contact: {}", *this, cc);
        auto intros = cc.intros();
        _intros.assign(intros.begin(), intros.end());
        log::trace(logcat, "New client intros: {}", fmt::join(_intros, ", "));
        _pivots.clear();
        for (auto& i : _intros)
            _pivots.insert(i.relay);

        update_paths(llarp::time_now_ms());
    }

    void OutboundClientSession::update_paths(std::chrono::milliseconds now)
    {
        // - If we have any current path to a pivot that is no longer in the client contact, kill
        //   it.
        // - If we killed our currently active path then switch to another.
        // - If we end up with too few paths then start some builds.

        Lock_t l(paths_mutex);

        if (!_intro_update_processed)
        {
            // Intros updated since we last updated, so we may have paths to pivots that are no
            // longer valid and need to be dropped

            std::list<path::Path*> drop;  // Use a list because it isn't valid to drop while iterating
            for (auto& p : paths())
                if (!_pivots.count(p.terminal_rid()))
                    drop.push_back(&p);

            for (auto* drop : drop)
                drop_path(*drop);

            _intro_update_processed = true;
        }

        int n_paths = num_paths();

        if (_current_path && _current_path->is_dead)
        {
            _current_path.reset();
            _dead_path = true;
        }

        if (!_current_path && n_paths)
            // We don't have a current path, possibly because we just dropped it in the above loop,
            // so select a new one to make our current path
            select_new_current();

        const auto& pathconf = router.config().paths;
        auto acceptable_ts = now + pathconf.acceptable_expiry;

        // To figure out how many new paths we ought to build we only consider existing paths that
        // are within our acceptable_paths window: anything older than that is due for replacement,
        // and will be dropped (but only once a replacement path is built).
        int needed = _target_paths - num_paths(acceptable_ts);
        if (_current_path)
        {
            // If we are currently on a path between min and acceptable, however, then we *don't*
            // need a replacement for it as we are sticking with it (until it reaches min expiry),
            // but it will have been counted in `needed` above
            if (auto curr_expires_in = _current_path->expires_in(now);
                curr_expires_in > pathconf.min_expiry && curr_expires_in < pathconf.acceptable_expiry)
                needed--;
        }
        if (needed <= 0)
            return;

        log::debug(
            logcat,
            "OutboundClientSession building {} paths to remote {} to reach target path count {}",
            needed,
            _remote,
            _target_paths);

        int count = 0;
        while (count < needed)
        {
            auto p = select_pivot();
            if (p)
            {
                if (auto* path_ptr = build_path_to_remote(p->first, p->second.first); path_ptr)
                {
                    path_ptr->aligned_hopid = p->second.second;
                    count++;
                }
            }
            else
                break;
        }

        log::debug(logcat, "Initiated {} path builds for {}", count, _remote);
    }

    void OutboundSession::session_init(path::Path& path)
    {
        oxenc::bt_dict_producer inner_btdp;

        inner_btdp.append("i"sv, _r.id().span());
        inner_btdp.append("p"sv, path.terminal_hopid().span());
        inner_btdp.append("t"sv, _inbound_tag);

        inner_btdp.append_signature("~", [this](std::span<const std::byte> to_sign) {
            std::array<std::byte, SIGSIZE> sig;
            _r.secret_key().sign(sig, to_sign);
            return sig;
        });

        auto inner_payload = std::move(inner_btdp).str();
        inner_payload.resize(inner_payload.size() + crypto::MAC_SIZE);
        auto [secret, eph_pk, dh_nonce] = crypto::dh_client_gen(_remote.router_id());
        _shared_secret = secret;
        crypto::xchacha20_poly1305_encrypt(inner_payload, secret, dh_nonce);

        oxenc::bt_dict_producer btdp;

        btdp.append("k", eph_pk.span());
        btdp.append("n", dh_nonce.span());
        btdp.append("x", inner_payload);

        send_session_data_message(btdp.span<std::byte>(), 0, true, true);
    }

    void OutboundSession::switch_path(path::Path& path, const HopID& pivot_hopid)
    {
        log::debug(logcat, "{} switching path to {} (hopid={})", *this, path, pivot_hopid);

        _current_path = path.shared_from_this();
        _dead_path = !_current_path;
        _remote_pivot_txid = path.aligned_hopid ? *path.aligned_hopid : path.terminal_hopid();

        if (!_is_established)
        {
            log::debug(
                logcat,
                "{} remote ({}) established, initiating session",
                _remote.client() ? "Aligned path for" : "Path to",
                _remote);

            session_init(path);
        }
        else
        {
            log::debug(
                logcat,
                "Dispatching path-switch request to remote {} to use hopid {} (for path {})",
                _remote,
                _remote_pivot_txid,
                path);

            oxenc::bt_dict_producer btdp;
            btdp.append("p"sv, path.terminal_hopid().span());
            send_session_control_message("path_switch"sv, btdp.span<std::byte>());
        }
    }

    void OutboundClientSession::select_new_current()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        // New path selection:
        //
        // Go look at all our current possible aligned paths to all introsets where both path and
        // introset expiry are at least [paths]:acceptable-expiry from now, and choose one of them
        // at random.
        //
        // If we can't find any suitable one, fallback to a random selection from any path+intro
        // within [paths]:min-expiry.
        //
        // If we don't have any of those, either, then we fail and disable the current path; when a
        // path build succeeds it will see that we have no current path and call this again to
        // select one.

        const auto min_exp = router.config().paths.min_expiry;
        const auto acceptable_exp = router.config().paths.acceptable_expiry;

        auto now = llarp::time_now_ms();
        std::vector<std::pair<path::Path*, HopID>> good, fallback;
        for (auto& path : active_paths())
        {
            auto path_expires_in = path.expires_in(now);
            if (path_expires_in < min_exp)
                continue;

            for (auto& intro : _intros)
            {
                if (intro.relay != path.terminal_rid())
                    continue;
                auto intro_expires_in = intro.expires_in(now);
                if (intro_expires_in < min_exp)
                    continue;

                auto expires_in = std::min(path_expires_in, intro_expires_in);
                auto& container = expires_in >= acceptable_exp ? good : fallback;

                container.emplace_back(&path, intro.hop);
            }
        }

        select_new_current_impl(std::move(good), std::move(fallback));
    }

    nlohmann::json OutboundClientSession::ExtractStatus() const
    {
        auto obj = path::PathHandler::ExtractStatus();
        // obj["lastExitUse"] = to_json(_last_use);
        //  auto pub = _auth->session_key().to_pubkey();
        //  obj["exitIdentity"] = pub.to_string();
        obj["endpoint"] = _remote.to_string();
        return obj;
    }

    std::optional<std::pair<RouterID, std::pair<std::chrono::seconds, HopID>>> OutboundClientSession::select_pivot()
    {
        // We've been asked to select a new pivot to build a path to.  We select using various
        // criteria:
        //
        // - start with all pivots listed in the intro sets
        // - eliminate any that expire less than [paths]:acceptable-expiry from now
        // - find the pivot(s) with the fewer number of existing (already built or building) paths
        // - select randomly from those.
        //
        // Thus we spread out our pivots along all pivots in a CC not close to expiry, and only
        // start doubling up on a pivot if we need to maintain more paths than there are pivots.

        auto now = llarp::time_now_ms();
        std::unordered_map<RouterID, std::pair<std::chrono::seconds, HopID>> select_from;
        int min_path_count = std::numeric_limits<int>::max();
        auto acceptable_cutoff =
            std::chrono::sys_time<std::chrono::milliseconds>{now + router.config().paths.acceptable_expiry};
        for (auto& intro : _intros)
        {
            if (intro.expiry < acceptable_cutoff)
                continue;

            const int existing_count = static_cast<int>(std::ranges::count_if(
                paths(), [&intro](const path::Path& p) { return p.terminal_rid() == intro.relay; }));

            if (existing_count > min_path_count)
                // We already found a pivot with fewer paths, so we don't want this one
                continue;
            if (existing_count < min_path_count)
            {
                // This is better than anything we've seen before so discard everything and start
                // over with this one
                select_from.clear();
                min_path_count = existing_count;
            }
            // In the case of duplicate router ids, replace with the newer (longer?) expiry
            // and that Intro's HopID
            auto exp = std::min<std::chrono::seconds>(
                std::chrono::floor<std::chrono::seconds>(intro.expires_in(now)), path::MAX_LIFETIME);
            if (auto [it, inserted] = select_from.emplace(intro.relay, std::make_pair(exp, intro.hop));
                not inserted and it->second.first < exp)
            {
                it->second.first = exp;
                it->second.second = intro.hop;
            }
        }

        if (select_from.empty())
            return std::nullopt;

        return *std::next(
            select_from.begin(),
            std::uniform_int_distribution<int>{0, static_cast<int>(select_from.size()) - 1}(llarp::csrng));
    }

    void OutboundSession::on_path_build_success(int64_t /*build_id*/, path::Path& p)
    {
        log::debug(logcat, "{} path {} built successfully", _remote, p);
        assert(router.loop.inside());

        // If we don't have a current path then immediately switch to this built in.
        //
        // TODO FIXME: this will end up slightly biasing the first path that gets used towards
        // closer edges and shorter hops; perhaps we should add a slight delay before choosing an
        // initial path so that other paths we have a chance to finish building before we select a
        // new one?
        if (!_current_path || _current_path->is_dead)
            select_new_current();
    }

    void OutboundSession::on_path_build_failure(int64_t /*build_id*/, path::Path* /*p*/, bool timeout)
    {
        log::warning(
            logcat,
            "{} aligned path build failed: {}",
            _remote,
            timeout ? "build request timed out" : "path construction failed");
    }

    InboundSession::InboundSession(handlers::SessionEndpoint& parent) : Session{parent.router, parent} {}

    void InboundSession::session_init_accept()
    {
        oxenc::bt_dict_producer btdp;
        btdp.append("t"sv, _inbound_tag);
        send_session_control_message("session_accept"sv, btdp.span<std::byte>());
    }

    void Session::handle_session_accept(std::span<const std::byte>)
    {
        log::warning(logcat, "Received session accept message, but not an outbound session.");
    }

    void OutboundSession::handle_session_accept(std::span<const std::byte> params)
    {
        if (_is_established)
        {
            log::warning(logcat, "Received session accept message, but session already established.");
            return;
        }
        oxenc::bt_dict_consumer btdc{params};
        _outbound_tag = btdc.require<session_tag>("t"sv);

        log::debug(logcat, "Remote provided session tag: {}", _outbound_tag);

        log::trace(logcat, "Outbound session to {} successfully created.", remote());
        _is_established = true;

        if (pre_establish_data_queue)
        {
            for (const auto& d : *pre_establish_data_queue)
                send_session_data_message(std::span{d.data(), d.size() - 1}, static_cast<uint8_t>(d.back()));
            pre_establish_data_queue.reset();
        }

        fire_waiting(llarp::time_now_ms());
    }

    InboundClientSession::InboundClientSession(
        handlers::SessionEndpoint& parent, std::shared_ptr<path::Path> p, std::vector<std::byte>&& request)
        : InboundSession{parent}, _current_path{std::move(p)}
    {
        _dead_path = !_current_path;
        init(std::move(request));
    }

    InboundRelaySession::InboundRelaySession(
        handlers::SessionEndpoint& parent, std::shared_ptr<path::TransitHop> thop, std::vector<std::byte>&& request)
        : InboundSession{parent}, _current_thop{std::move(thop)}
    {
        _dead_path = !_current_thop;
        init(std::move(request));
    }

    void InboundRelaySession::encrypt_path_message(std::vector<std::byte>& data, SymmNonce&& nonce, std::byte type)
    {
        // This is similar to Path encrypt, except that we are operating at the far end of a
        // transithop and starting the encrypt backwards (and so don't see the whole path, just our
        // end of it) which means:
        // - we only do our first single layer of the required onioning
        // - we apply the xor_nonce *before* xchacha20 rather than after (because we are applying
        //   the operations in reverse).
        auto orig_size = data.size();
        data.resize(orig_size + path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD);
        static_assert(path::Path::ENCRYPT_PATH_MESSAGE_OVERHEAD == SymmNonce::SIZE + HopID::SIZE + 1);
        auto [inner_payload, bnonce, bhop, msgtype] = split_span_tail<SymmNonce::SIZE, HopID::SIZE, 1>(data);
        assert(inner_payload.size() == orig_size);

        nonce ^= _current_thop->xor_nonce;
        crypto::xchacha20(inner_payload, _current_thop->shared_secret, nonce);
        nonce.copy_to(bnonce);
        _current_thop->rxid.copy_to(bhop);
        msgtype[0] = type;
    }

    void InboundRelaySession::send_path_data_message(std::vector<std::byte>&& data, SymmNonce&& nonce)
    {
        update_active();
        if (check_dead(_current_thop, *this))
        {
            log::debug(logcat, "Unable to send return relay session data message: no current transit hop");
            return;
        }

        encrypt_path_message(data, std::move(nonce), path::Path::DATA_MESSAGE_TYPE);
        _parent.router.link_endpoint().send_datagram(_current_thop->downstream, std::move(data));
    }

    void InboundRelaySession::send_path_control_message(std::vector<std::byte>&& data, SymmNonce&& nonce)
    {
        update_active();
        if (check_dead(_current_thop, *this))
        {
            log::debug(logcat, "Unable to send return relay session control message: no current transit hop");
            return;
        }

        encrypt_path_message(data, std::move(nonce), path::Path::CONTROL_MESSAGE_TYPE);
        _parent.router.link_endpoint().send_command(
            _current_thop->downstream, "session_control"s, std::move(data), nullptr);
    }
}  // namespace llarp::session
