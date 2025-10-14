#pragma once

#include <llarp/address/address.hpp>
#include <llarp/constants/path.hpp>
#include <llarp/ev/tcp.hpp>
#include <llarp/ev/udp.hpp>
#include <llarp/net/ip_packet.hpp>
#include <llarp/path/path.hpp>
#include <llarp/path/path_handler.hpp>
#include <llarp/path/transit_hop.hpp>
#include <llarp/util/bspan.hpp>

#include <oxen/quic/btstream.hpp>
#include <oxen/quic/connection.hpp>
#include <oxen/quic/endpoint.hpp>

#include <chrono>
#include <queue>

namespace llarp
{
    namespace quic = oxen::quic;

    using recv_session_dgram_cb = std::function<void(std::span<std::byte>)>;

    inline constexpr auto SESSION_PATH_BUILD_ATTEMPTS{3};

    namespace link
    {
        class TunnelManager;
    }  //  namespace link

    namespace handlers
    {
        class SessionEndpoint;
    }  // namespace handlers

    /** Snode vs Client Session
        - client to client: shared secret (symmetric key) is negotiated
        - client to relay:
          - the traffic to the pivot is encrypted
          - the pivot is the terminus, so data doesn't need to be encrypted
    */

    namespace session
    {
        using session_tag = uint32_t;

        struct TCPTunnel;

        // We must not use the same nonce for path switch and session init, as they can be in the
        // same message using the same shared secret.  As such, the path switch message will use
        // dh_nonce ^ this xor factor.
        inline const SymmNonce switch_xor_factor = SymmNonce::filled<SymmNonce>(0x42);

        class Session
        {
            // TODO FIXME: how long since last use should is_expired() return true?
            static constexpr std::chrono::milliseconds SESSION_TIMEOUT = 30s;

            friend struct TCPTunnel;
            template <typename T>
            friend bool check_dead(std::shared_ptr<T>& path_like, Session& s);

          protected:
            Router& _r;
            handlers::SessionEndpoint& _parent;

            // The session tags.  Each side of the session decides its own inbound tag, meaning
            // no worries about collision *and* shorter tags on each packet.
            session_tag _inbound_tag;
            session_tag _outbound_tag;

            NetworkAddress _remote;

            PubKey dh_pk;
            SymmNonce dh_nonce;
            SharedSecret _shared_secret;

            // used for bridging data messages across aligned paths
            HopID _remote_pivot_txid;

            // Will be set to true when an outbound session is established; will always be true for
            // inbound sessions.
            bool _is_established{false};

            // Will be set to true if this session has been closed (i.e. via a call to
            // SessionEndpoint::close_session).  Closing is terminal (i.e. a closed Session instance
            // will never become non-closed; reestablishing a closed Session requires replacing it).
            bool _is_closed{false};

            // Set to true if our current path is definitely dead, to short-circuit things like
            // send_session_data_message encryption if we know we can't deliver it anywhere.  This
            // is roughly equivalent to `!path || path->is_dead`, except that the base class doesn't
            // know about `path` and so this allows subclasses the provide the information back to
            // the base class without needing an extra virtual method call on every packet.
            //
            // Base classes should reset this to false as soon as they switch to a new path.
            bool _dead_path{true};

            std::unique_ptr<TCPTunnel> tcp_tunnel{nullptr};

            // for tunneled clients, maps remote dest port to udp socket
            // for return traffic, dest port will be the client's udp socket port
            std::unordered_map<uint16_t, std::unique_ptr<quic::UDPSocket>> udp_handles;

            // bidirectional map, obfuscating the randomized source port from the user and
            // mapping that obfuscated port back to that obfuscated port for return traffic.
            // This is both to track used ports so we don't accept traffic to an unmapped
            // one, as well as in case port selection is fingerprintable.
            // udp_client_ports maps client source port -> pseudo source port
            // udp_remote_ports maps pseudo dest port -> client dest port
            std::unordered_map<uint16_t, uint16_t> udp_client_ports;
            std::unordered_map<uint16_t, uint16_t> udp_remote_ports;
            uint16_t next_udp_client_port{1024};
            std::chrono::milliseconds last_activity = llarp::time_now_ms();

            // only currently useful for outbound client sessions, but more convenient here
            // than an overload on all inbound traffic functions for that one case
            std::chrono::milliseconds last_inbound_activity = llarp::time_now_ms();

            void update_active();

            // We capture a weak_ptr to this shared_ptr to avoid needing to use shared_from_this
            // when we need to assure we are still alive in lambdas given to external objects.  I.e.
            // this allows: `[alive=canary(), this] { if (!alive.lock()) return; ... }`
            std::shared_ptr<bool> _destructor_canary{std::make_shared<bool>(true)};
            std::weak_ptr<bool> canary() { return _destructor_canary; }

            Session(
                Router& r, handlers::SessionEndpoint& parent, const NetworkAddress& remote, session_tag inbound_tag);

            Session(Router& r, handlers::SessionEndpoint& parent);

            virtual void handle_client_contact(std::span<const std::byte> payload);

            virtual ~Session();

          public:
            // Non-movable, non-copyable:
            Session(Session&&) = delete;
            Session(const Session&) = delete;
            Session& operator=(Session&&) = delete;
            Session& operator=(const Session&) = delete;

            // True if this is an OutboundSession-derived instance.
            const bool is_outbound;

            // True if this is a session instance between a client and relay (i.e. either
            // InboundRelaySession- or OutboundRelaySession-derived).
            const bool is_relay_session;

            // TODO FIXME: make this do something.  When the session establishes we should get some
            // capabilities metadata, such as whether it supports exit.
            const bool is_exit_capable{false};

            const NetworkAddress& remote() const { return _remote; }

            std::string encode_session_control_message(
                std::string_view method,
                std::span<const std::byte> body,
                const SymmNonce& nonce,
                std::optional<HopID> pivot_id);

            // Attempts to send a session control message down the current path.  Returns false
            // (without calling `func`) if there is no current path, otherwise returns true
            bool send_session_control_message(std::string_view method, std::span<const std::byte> body);

            void recv_session_control_message(
                std::vector<std::byte>&& message,
                const SymmNonce& nonce,
                std::variant<std::shared_ptr<path::TransitHop>, std::shared_ptr<path::Path>> source);

            virtual void handle_session_accept(std::span<const std::byte> params);

            void send_session_data_message(std::span<const std::byte> data, net::IPProtocol proto);
            std::optional<std::pair<std::vector<std::byte>, SymmNonce>> make_session_data_message(
                std::span<const std::byte> data,
                uint8_t type,
                bool control = false,
                bool init = false,
                SymmNonce nonce = SymmNonce::make_random());
            void send_session_data_message(
                std::span<const std::byte> data, uint8_t type, bool control = false, bool init = false);

            virtual void send_path_data_message(std::vector<std::byte>&& data, SymmNonce&& nonce) = 0;
            virtual void send_path_control_message(
                std::vector<std::byte>&& data, SymmNonce&& nonce, bool path_switch) = 0;

            // Called by send_session_data_message if trying to send a data message on a
            // not-yet-established connection (which, by definition, can only be an outbound
            // session).  The default does nothing, but OutboundSession overrides to queue them (up
            // to a limit) so that initially sent packets on an initializing session get delivered
            // as soon as the session establishes.  This allows, for example, pings to get delivered
            // rather than having the first couple getting dropped before establishing.
            virtual void queue_data_message(std::span<const std::byte> /*data*/, uint8_t /*type*/) {}

            void recv_session_data_message(std::vector<std::byte> data, const SymmNonce& nonce);

            void publish_client_contact(const EncryptedClientContact& ecc);

            void handle_udp_from_remote(IPPacket&& pkt);

            uint16_t setup_udp_mapping(uint16_t dest_port);

            uint16_t map_tcp_remote_port(uint16_t dest_port);

            // Returns true if this session is established, and has not been explicitly closed.
            // Inbound sessions are instantly established; outbound sessions are established once
            // the session init response arrives from the remote.
            bool is_established() const;

            session_tag inbound_tag() const { return _inbound_tag; }
            session_tag outbound_tag() const { return _outbound_tag; }

            // Returns true if this session has been closed, i.e. it is in the middle of shutting
            // down.
            bool is_closed() const { return _is_closed; }

            // Called to close this session.  If the bool is true then the session will attempt to
            // send a session_close control message down the active path.
            void close(bool send_close);

            virtual void recv_close();

            bool is_expired(std::chrono::milliseconds now) const;

            virtual std::string to_string() const = 0;

            static constexpr bool to_string_formattable = true;

            // Called periodically (somewhere under Router::tick) to handle anything needed on the
            // session, but also sometimes called in other places (e.g. if we need new paths ASAP
            // rather than waiting for the next tick)
            virtual void tick([[maybe_unused]] std::chrono::milliseconds now) {}
        };

        class OutboundSession : public path::PathHandler, public Session
        {
          protected:
            std::shared_ptr<path::Path> _current_path;

            OutboundSession(
                const NetworkAddress& remote,
                handlers::SessionEndpoint& parent,
                int num_hops,
                session_tag inbound_tag,
                std::function<void(OutboundSession& session)> on_established,
                std::optional<std::chrono::milliseconds> establish_timeout = std::nullopt);

            ~OutboundSession() override = default;

            void select_new_current_impl(
                std::vector<std::pair<path::Path*, HopID>>&& good,
                std::vector<std::pair<path::Path*, HopID>>&& fallback);

            void tick(std::chrono::milliseconds now) override;

            virtual void select_new_current() = 0;

            // Closes non-active paths that are close to expiry, i.e. any paths that we would not
            // select if we need to switch paths.
            void close_old_paths(std::chrono::milliseconds now);

            void send_path_data_message(std::vector<std::byte>&& data, SymmNonce&& nonce) override;
            void send_path_control_message(std::vector<std::byte>&& data, SymmNonce&& nonce, bool path_switch) override;

            void queue_data_message(std::span<const std::byte>, uint8_t type) override;

            // We stash the `type` as the last byte of the vector
            std::optional<std::deque<std::vector<std::byte>>> pre_establish_data_queue;

          private:
            // Switches to (or starts using) the given path.
            void switch_path(path::Path& p, const HopID& new_pivot_txid);

            std::string make_session_init(path::Path& path);

            void fire_waiting(std::chrono::milliseconds now);

            using active_item = std::pair<std::chrono::milliseconds, std::function<void(OutboundSession& session)>>;
            struct on_established_sorter
            {
                bool operator()(const active_item& a, const active_item& b) const { return a.first > b.first; }
            };
            // Callbacks that we fire once we achieve active status (i.e. at least one established
            // path for this session), or time out.  The key is the `llarp::time_now_ms()` expiry
            // time after which we should give up and fire the callback anyway.  The callback can
            // figure out which case this was by checking `session.is_active()`.
            std::priority_queue<active_item, std::vector<active_item>, on_established_sorter> _on_established;

            void on_path_build_success(int64_t build_id, path::Path& p) override;

            void on_path_build_failure(int64_t build_id, path::Path* p, bool timeout) override;

            void handle_session_accept(std::span<const std::byte> params) override;

          public:
            // void stop_session() override;

            // Calls the given callback with the session when it becomes established, or after
            // timing out.  (The callback can check which case occured via `.is_established()` on
            // the argument).  If the session is already established when this is called then it is
            // fired immediately (before returning).
            //
            // If the timeout is omitted then it defaults to the config
            // [network]path-alignment-timeout setting.
            //
            // Multiple callbacks waiting on the same session are permitted.
            void on_established(
                std::function<void(OutboundSession&)> callback,
                std::optional<std::chrono::milliseconds> timeout = std::nullopt);

            std::string to_string() const override;

            inline static constexpr int MAX_QUEUED_PACKETS = 30;
        };

        // Outbound Session to Remote Relay
        class OutboundRelaySession final : public OutboundSession
        {
          public:
            OutboundRelaySession(
                const NetworkAddress& remote,
                handlers::SessionEndpoint& parent,
                session_tag inbound_tag,
                std::function<void(OutboundSession& session)> on_established,
                std::optional<std::chrono::milliseconds> establish_timeout = std::nullopt);

            void update_paths(std::chrono::milliseconds now) override;

            void recv_close() override;

            // void stop(bool send_close = false) override;

          private:
            void select_new_current() override;
        };

        // Outbound Session to Remote Client
        class OutboundClientSession final : public OutboundSession
        {
          public:
            OutboundClientSession(
                const NetworkAddress& remote,
                handlers::SessionEndpoint& parent,
                session_tag inbound_tag,
                std::function<void(OutboundSession& session)> on_established,
                std::optional<std::chrono::milliseconds> establish_timeout = std::nullopt);

          private:
            std::vector<ClientIntro> _intros;
            std::unordered_set<RouterID> _pivots;
            bool _intro_update_processed = false;
            bool updating_intros = false;

            std::chrono::milliseconds last_cc_update = 0s;
            std::optional<ClientContact> current_cc{std::nullopt};
            bool cc_ok = false;

            // Chooses the next router id to pivot to, based on introset and current paths.  Returns
            // nullopt if no pivot is available right now, otherwise the router id and the lifetime
            // of paths to that pivot (so that we avoid creating paths that will become stale paths
            // living beyond the expiry of the pivot).
            std::optional<std::pair<RouterID, std::pair<std::chrono::seconds, HopID>>> select_pivot();

            void select_new_current() override;

          protected:
            void handle_client_contact(std::span<const std::byte> payload) override;

          public:
            // Initiates a client intro lookup via the session endpoint.  This can be called even if
            // there already is intros, to refresh/replace them.
            void refresh_intros();

            void tick(std::chrono::milliseconds now) override;

            // Called with a client contact to replace the current set of client intros used by this
            // session with the ones in the given client contact.  This is called by
            // `refresh_intros()` upon a success fetch, but can also be called externally (such as
            // when receiving intro updates through an existing session).
            void update_intros(const ClientContact& cc);

            void update_paths(std::chrono::milliseconds now) override;

            void recv_close() override;

            nlohmann::json ExtractStatus() const;

            const RouterID& remote_endpoint() const { return _remote.router_id(); }
        };

        class InboundSession : public Session
        {
          protected:
            InboundSession(handlers::SessionEndpoint& parent);

            ~InboundSession() override = default;

            void init(std::vector<std::byte>&& request);

          public:
            void session_init_accept();
        };

        // Inbound Session *to* client from client (we are the target client)
        class InboundClientSession final : public InboundSession
        {
            std::shared_ptr<path::Path> _current_path;

            void send_path_data_message(std::vector<std::byte>&& data, SymmNonce&& nonce) override;
            void send_path_control_message(std::vector<std::byte>&& data, SymmNonce&& nonce, bool path_switch) override;

          public:
            InboundClientSession(
                handlers::SessionEndpoint& parent, std::shared_ptr<path::Path> p, std::vector<std::byte>&& request);

            void handle_path_switch(HopID pivot, std::shared_ptr<path::Path> path);

            std::string to_string() const override;
        };

        // Inbound Session *to* relay from client (we are the target relay)
        class InboundRelaySession final : public InboundSession
        {
            std::shared_ptr<path::TransitHop> _current_thop;

            void encrypt_path_message(std::vector<std::byte>& data, SymmNonce&& nonce, std::byte type);

            void send_path_data_message(std::vector<std::byte>&& data, SymmNonce&& nonce) override;

            void send_path_control_message(std::vector<std::byte>&& data, SymmNonce&& nonce, bool path_switch) override;

          public:
            InboundRelaySession(
                handlers::SessionEndpoint& parent,
                std::shared_ptr<path::TransitHop> thop,
                std::vector<std::byte>&& request);

            void handle_path_switch(HopID pivot, std::shared_ptr<path::TransitHop> thop);

            std::string to_string() const override;
        };

    }  // namespace session

}  // namespace llarp
