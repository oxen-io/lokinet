#pragma once

#include <llarp/address/address.hpp>
#include <llarp/constants/path.hpp>
#include <llarp/contact/tag.hpp>
#include <llarp/ev/tcp.hpp>
#include <llarp/ev/udp.hpp>
#include <llarp/net/ip_packet.hpp>
#include <llarp/path/path.hpp>

#include <oxen/quic/btstream.hpp>
#include <oxen/quic/connection.hpp>
#include <oxen/quic/endpoint.hpp>

namespace llarp
{
    // FIXME: have this hook give an error string on failure, not just false
    using on_session_init_hook = std::function<void(bool)>;
    using recv_session_dgram_cb = std::function<void(std::span<std::byte>)>;

    inline constexpr size_t PATHS_PER_INTRO{2};
    inline constexpr auto SESSION_PATH_BUILD_ATTEMPTS{3};

    namespace link
    {
        class TunnelManager;
    }  //  namespace link

    namespace handlers
    {
        class SessionEndpoint;
    }  // namespace handlers

    using intro_path_map = std::map<ClientIntro, path::PathPtrSet, ClientIntroExpComp>;

    /** Snode vs Client Session
        - client to client: shared secret (symmetric key) is negotiated
        - client to snode:
          - the traffic to the pivot is encrypted
          - the pivot is the terminus, so data doesn't need to be encrypted
    */

    namespace session
    {
        struct BaseSession
        {
          protected:
            Router& _r;
            handlers::SessionEndpoint& _parent;

            session_tag _tag;
            NetworkAddress _remote;

            std::unique_ptr<shared_kx_data> session_keys{};

            // used for bridging data messages across aligned paths
            HopID _remote_pivot_txid;

            const bool _use_tun{};
            const bool _is_outbound{};
            const bool _is_snode_session{};
            const bool _is_exit_session{};

            bool _is_active{};

            std::shared_ptr<session_path_interface> _current_path;
            HopID _pivot_txid;

            recv_session_dgram_cb _recv_dgram;

            // manually routed QUIC endpoint
            std::shared_ptr<quic::Endpoint> _ep;

            std::shared_ptr<quic::connection_interface> _ci;

            // TCPHandle listeners mapped to the local port they are bound on
            std::unordered_map<uint16_t, std::shared_ptr<TCPHandle>> _handles;

            std::unordered_set<std::shared_ptr<TCPConnection>> _tcp_conns;

            void _init_ep();

            // for tunneled clients, maps remote dest port to udp socket
            // for return traffic, dest port will be the client's udp socket port
            std::unordered_map<uint16_t, std::unique_ptr<quic::UDPSocket>> udp_handles;

          public:
            BaseSession(
                Router& r,
                std::shared_ptr<session_path_interface> _p,
                handlers::SessionEndpoint& parent,
                NetworkAddress remote,
                HopID remote_pivot_txid,
                session_tag _t,
                bool use_tun,
                bool is_outbound,
                shared_kx_data kx_data);

            virtual ~BaseSession();

            bool is_outbound() const { return _is_outbound; }

            const NetworkAddress& remote() const { return _remote; }

            NetworkAddress remote() { return _remote; }

            virtual bool send_path_control_message(
                std::string_view method, std::span<const std::byte> body, std::function<void(quic::message)> func);

            // NB: mutates data (encrypting in place)
            virtual bool send_path_data_message(std::span<std::byte> data);

            // NB: mutates data (decrypting in place)
            void recv_path_data_message(std::span<std::byte> data);

            void set_new_current_path_interface(std::shared_ptr<session_path_interface> _new_path);

            void set_remote_pivot_tx(HopID new_remote_txid);

            void publish_client_contact(const EncryptedClientContact& ecc, std::function<void(quic::message)> func);

            bool using_tun() const { return _use_tun; }

            // inbound
            void tcp_backend_connect();

            // outbound
            void tcp_backend_listen(on_session_init_hook cb, uint16_t port = 0);

            void handle_udp_from_remote(IPPacket&& pkt);

            uint16_t setup_udp_mapping(uint16_t dest_port);

            session_tag tag() { return _tag; }

            const session_tag& tag() const { return _tag; }

            bool is_exit_session() const { return _is_exit_session; }

            bool is_active() const { return _is_active; }

            void activate();

            void deactivate();

            virtual void stop_session(bool send_close = false, std::function<void(quic::message)> func = nullptr);

            void send_path_close(std::function<void(quic::message)> func = nullptr);

            virtual std::string to_string() const;

            static constexpr bool to_string_formattable = true;

            // These methods do nothing by default; outbound sessions subclasses override to do something.
            virtual void tick_outbound(std::chrono::milliseconds /*now*/) {}
            virtual void update_outbound_remote_intros(sorted_intro_set /*intros*/) {}
        };

        // Outbound Session to Remote Relay
        struct OutboundRelaySession : public path::PathHandler, public BaseSession
        {
            OutboundRelaySession(
                NetworkAddress _remote,
                handlers::SessionEndpoint& parent,
                std::shared_ptr<path::Path> path,
                session_tag _t,
                HopID remote_pivot_txid,
                shared_kx_data kx_data);

          protected:
            std::chrono::milliseconds _last_use;

            void rotate_paths() override;

            void drop_oldest_path() override;

            void select_new_current();

            void switch_to_new_path(std::shared_ptr<path::Path> p);

            std::shared_ptr<path::Path> current_path();

          public:
            std::shared_ptr<path::PathHandler> get_self() override;

            std::weak_ptr<path::PathHandler> get_weak() override;

            bool send_path_control_message(
                std::string_view method,
                std::span<const std::byte> body,
                std::function<void(quic::message)> func) override;

            bool send_path_data_message(std::span<std::byte> data) override;

            void build_more(size_t n = 0) override;

            void send_path_switch();

            void stop(bool send_close = false) override;

            void stop_session(bool send_close = false, std::function<void(quic::message)> func = nullptr) override;

            void tick_outbound(std::chrono::milliseconds now) override { tick(now); }
        };

        // Outbound Session to Remote Client
        struct OutboundClientSession final : public OutboundRelaySession
        {
            OutboundClientSession(
                NetworkAddress _remote,
                handlers::SessionEndpoint& parent,
                std::shared_ptr<path::Path> path,
                HopID remote_pivot_txid,
                session_tag _t,
                sorted_intro_set cc,
                shared_kx_data kx_data);

          private:
            intro_path_map intro_path_mapping{};

            void populate_intro_map(const sorted_intro_set& intros);

            bool update_local_paths();

            void build_and_switch_paths();

            void build_and_switch_paths(sorted_intro_set intros);

            void switch_to_new_path(std::shared_ptr<path::Path> p, HopID new_pivot_txid);

            bool select_new_current();

            void map_path(const std::shared_ptr<path::Path>& p);

            bool unmap_path(const std::shared_ptr<path::Path>& p);

          protected:
            void rotate_paths() override;

            void drop_oldest_path() override;

          public:
            std::shared_ptr<path::PathHandler> get_self() override;

            std::weak_ptr<path::PathHandler> get_weak() override;

            bool send_path_control_message(
                std::string_view method,
                std::span<const std::byte> body,
                std::function<void(quic::message)> func) override;

            bool send_path_data_message(std::span<std::byte> data) override;

            void update_outbound_remote_intros(sorted_intro_set intros) override;

            void build_more(size_t n = 0) override;

            nlohmann::json ExtractStatus() const;

            void path_build_succeeded(const std::shared_ptr<path::Path>& p) override;

            void path_build_failed(const std::shared_ptr<path::Path>& p, bool timeout = false) override;

            void stop_session(bool send_close = false, std::function<void(quic::message)> func = nullptr) override;

            bool is_ready() const;

            const RouterID& remote_endpoint() const { return _remote.router_id(); }

            bool is_expired(std::chrono::milliseconds now) const;
        };

        // Inbound Session to Local Client
        struct InboundClientSession : public BaseSession
        {
            InboundClientSession(
                NetworkAddress _remote,
                std::shared_ptr<session_path_interface> _p,
                handlers::SessionEndpoint& parent,
                HopID remote_pivot_txid,
                session_tag _t,
                bool use_tun,
                shared_kx_data kx_data);

            void recv_path_switch(HopID remote_pivot_txid, std::shared_ptr<session_path_interface> new_pi);
        };

        // Inbound Session to Local Relay
        struct InboundRelaySession final : public InboundClientSession
        {
            InboundRelaySession(
                NetworkAddress _remote,
                std::shared_ptr<session_path_interface> _p,
                handlers::SessionEndpoint& parent,
                HopID remote_pivot_txid,
                session_tag _t,
                bool use_tun,
                shared_kx_data kx_data);

            bool send_path_control_message(
                std::string_view method,
                std::span<const std::byte> body,
                std::function<void(quic::message)> func) override;

            bool send_path_data_message(std::span<std::byte> data) override;
        };

    }  // namespace session

}  // namespace llarp
