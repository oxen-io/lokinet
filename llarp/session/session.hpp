#pragma once

#include <llarp/address/address.hpp>
#include <llarp/constants/path.hpp>
#include <llarp/contact/tag.hpp>
#include <llarp/ev/tcp.hpp>
#include <llarp/ev/udp.hpp>
#include <llarp/net/ip_packet.hpp>
#include <llarp/path/path.hpp>
#include <llarp/path/path_handler.hpp>

#include <oxen/quic/btstream.hpp>
#include <oxen/quic/connection.hpp>
#include <oxen/quic/endpoint.hpp>

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
        struct TCPTunnel;

        class Session
        {
            friend struct TCPTunnel;

          protected:
            Router& _r;
            handlers::SessionEndpoint& _parent;

            session_tag _tag;
            NetworkAddress _remote;

            SharedSecret _shared_secret;

            // used for bridging data messages across aligned paths
            HopID _remote_pivot_txid;

            // Will be set to true when a session is established:
            bool _is_established{false};
            // Will be set to true if this session has been closed (i.e. via a call to
            // SessionEndpoint::close_session).  Closing is terminal (i.e. a closed Session instance
            // will never become non-closed; reestablishing a closed Session requires replacing it).
            bool _is_closed{false};

            std::weak_ptr<session_path_interface> _current_path;

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

            // We capture a weak_ptr to this shared_ptr to avoid needing to use shared_from_this
            // when we need to assure we are still alive in lambdas given to external objects.  I.e.
            // this allows: `[alive=canary(), this] { if (!alive.lock()) return; ... }`
            std::shared_ptr<bool> _destructor_canary{std::make_shared<bool>(true)};
            std::weak_ptr<bool> canary() { return _destructor_canary; }

            Session(
                Router& r, handlers::SessionEndpoint& parent, const NetworkAddress& remote);

            Session(
                Router& r,
                handlers::SessionEndpoint& parent,
                const NetworkAddress& remote,
                const SharedSecret& secret,
                const session_tag& t,
                std::weak_ptr<session_path_interface> path,
                const HopID& remote_pivot_txid);

          public:
            virtual ~Session();

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

            // Attempts to send a session control message down the current path.  Returns false
            // (without calling `func`) if there is no current path, otherwise returns true and,
            // when a response arrives (or timeout occurs), `func` will be called with the response.
            virtual bool send_session_control_message(
                std::string_view method,
                std::span<const std::byte> body,
                std::function<void(quic::message)> func = nullptr);

            void send_session_data_message(std::span<const std::byte> data, net::IPProtocol proto);
            void send_session_data_message(std::span<const std::byte> data, uint8_t type);

            void recv_session_data_message(std::vector<std::byte> data, const SymmNonce& nonce);

            void publish_client_contact(const EncryptedClientContact& ecc, std::function<void(quic::message)> func);

            void handle_udp_from_remote(IPPacket&& pkt);

            uint16_t setup_udp_mapping(uint16_t dest_port);

            uint16_t map_tcp_remote_port(uint16_t dest_port);

            // Returns true if this session is established, and has not been explicitly closed.
            // Inbound sessions are instantly established; outbound sessions are established once
            // the session init response arrives from the remote.
            bool is_established() const;

            // The session tag.  This value is only meaningful once the session is established.
            const session_tag& tag() const { return _tag; }

            // Returns true if this session has been closed, i.e. it is in the middle of shutting
            // down.
            bool is_closed() const { return _is_closed; }

            // Called to close this session.  If the bool is true then the session will attempt to
            // send a session_close control message down the active path.
            void close(bool send_close);

            virtual std::string to_string() const;

            static constexpr bool to_string_formattable = true;

            // Called periodically (somewhere under Router::tick) to handle anything needed on the
            // session.
            virtual void tick([[maybe_unused]] std::chrono::milliseconds now) {};
        };

        class OutboundSession : public path::PathHandler, public Session
        {
          protected:
            OutboundSession(
                const NetworkAddress& remote,
                handlers::SessionEndpoint& parent,
                int num_hops,
                std::function<void(OutboundSession& session)> on_established);

            void select_new_current_impl(
                std::vector<std::pair<path::Path*, HopID>>&& good,
                std::vector<std::pair<path::Path*, HopID>>&& fallback);

            // Switches (or starts using) the given path.  If there currently is no path, this will
            // call any waiting on-established callbacks.
            void switch_path(path::Path& p, const HopID& new_pivot_txid);

            void tick(std::chrono::milliseconds now) override;

            // TODO FIXME: these were doing nothing useful, but I think we need them to do something
            // useful.
            //
            // std::chrono::milliseconds _last_use;
            // bool is_expired(std::chrono::milliseconds now) const;

          private:
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

          public:
            // void stop_session() override;

            std::shared_ptr<path::Path> current_path();

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
        };

        // Outbound Session to Remote Relay
        class OutboundRelaySession : public OutboundSession
        {
          public:
            OutboundRelaySession(
                const NetworkAddress& remote,
                handlers::SessionEndpoint& parent,
                std::function<void(OutboundSession& session)> on_active);

            bool send_session_control_message(
                std::string_view method,
                std::span<const std::byte> body,
                std::function<void(quic::message)> func) override;

            void update_paths() override;

            // void stop(bool send_close = false) override;

          private:
            void select_new_current();
        };

        // Outbound Session to Remote Client
        class OutboundClientSession : public OutboundSession
        {
          public:
            OutboundClientSession(
                const NetworkAddress& remote,
                handlers::SessionEndpoint& parent,
                std::function<void(OutboundSession& session)> on_established);

          private:
            std::vector<ClientIntro> _intros;
            std::unordered_set<RouterID> _pivots;
            bool _intro_update_processed = false;

            // Chooses the next router id to pivot to, based on introset and current paths.  Returns
            // nullopt if no pivot is available right now.
            std::optional<RouterID> select_pivot();

            void select_new_current();

            void on_path_build_success(int64_t build_id, path::Path& p) override;

            void on_path_build_failure(int64_t build_id, path::Path* p, bool timeout) override;

          public:
            // Initiates a client intro lookup via the session endpoint.  This can be called even if
            // there already is intros, to refresh/replace them.
            void refresh_intros();

            // Called with a client contact to replace the current set of client intros used by this
            // session with the ones in the given client contact.  This is called by
            // `refresh_intros()` upon a success fetch, but can also be called externally (such as
            // when receiving intro updates through an existing session).
            void update_intros(const ClientContact& cc);

            void update_paths() override;

            nlohmann::json ExtractStatus() const;

            const RouterID& remote_endpoint() const { return _remote.router_id(); }
        };

        class InboundSession : public Session
        {
          public:
            // FIXME: this was protected, but I guess the compiler doesn't like exposing it as
            // public in the calls below via a `using` declaration?
            InboundSession(
                const NetworkAddress& remote,
                handlers::SessionEndpoint& parent,
                const session_tag& t,
                const SharedSecret& secret,
                std::weak_ptr<session_path_interface> p,
                const HopID& remote_pivot_txid);
        };

        // Inbound Session *to* client from client (we are the target client)
        class InboundClientSession : public InboundSession
        {
          public:
            using InboundSession::InboundSession;

            void recv_path_switch(const HopID& remote_pivot_txid, std::weak_ptr<session_path_interface> new_pi);
        };

        // Inbound Session *to* relay from client (we are the target relay)
        class InboundRelaySession final : public InboundSession
        {
          public:
            using InboundSession::InboundSession;

            bool send_session_control_message(
                std::string_view method,
                std::span<const std::byte> body,
                std::function<void(quic::message)> func) override;
        };

    }  // namespace session

}  // namespace llarp
