#pragma once

#include <llarp/address/address.hpp>
#include <llarp/constants/path.hpp>
#include <llarp/contact/tag.hpp>
#include <llarp/ev/tcp.hpp>
#include <llarp/ev/udp.hpp>
#include <llarp/net/ip_packet.hpp>
#include <llarp/path/path.hpp>

#include <oxen/quic.hpp>

#include <deque>
#include <queue>

namespace llarp
{
    // FIXME: have this hook give an error string on failure, not just false
    using on_session_init_hook = std::function<void(bool)>;
    using recv_session_dgram_cb = std::function<void(std::vector<std::byte>)>;

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
        struct TCPTunnel;

        struct BaseSession
        {
            friend struct TCPTunnel;
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

            std::unique_ptr<TCPTunnel> tcp_tunnel{nullptr};

            // for tunneled clients, maps remote dest port to udp socket
            // for return traffic, dest port will be the client's udp socket port
            std::unordered_map<uint16_t, std::unique_ptr<UDPHandle>> udp_handles;

            // bidirectional map, obfuscating the randomized source port from the user and
            // mapping that obfuscated port back to that obfuscated port for return traffic.
            // This is both to track used ports so we don't accept traffic to an unmapped
            // one, as well as in case port selection is fingerprintable.
            // udp_client_ports maps client source port -> pseudo source port
            // udp_remote_ports maps pseudo dest port -> client dest port
            std::unordered_map<uint16_t, uint16_t> udp_client_ports;
            std::unordered_map<uint16_t, uint16_t> udp_remote_ports;
            uint16_t next_udp_client_port{1024};

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

            virtual bool send_path_control_message(std::string method, std::string body, bt_control_response_hook func);

            // For sending raw IP packets (e.g. straight from tun device), converts
            // the IPProtocol to a session::traffic_type (or RAW if not present in traffic_type)
            bool send_path_data_message(std::string data, net::IPProtocol proto);

            bool send_path_data_message(std::string data, uint8_t type);

            void recv_path_data_message(std::vector<std::byte> data);

            void set_new_current_path_interface(std::shared_ptr<session_path_interface> _new_path);

            void set_remote_pivot_tx(HopID new_remote_txid);

            void publish_client_contact(const EncryptedClientContact& ecc, bt_control_response_hook func);

            bool using_tun() const { return _use_tun; }

            void handle_udp_from_remote(IPPacket&& pkt);

            uint16_t setup_udp_mapping(uint16_t dest_port);

            uint16_t map_tcp_remote_port(uint16_t dest_port);

            session_tag tag() { return _tag; }

            const session_tag& tag() const { return _tag; }

            bool is_exit_session() const { return _is_exit_session; }

            bool is_active() const { return _is_active; }

            void activate();

            void deactivate();

            virtual void stop_session(bool send_close = false, bt_control_response_hook func = nullptr);

            void send_path_close(bt_control_response_hook func = nullptr);

            virtual std::string to_string() const;

            static constexpr bool to_string_formattable = true;
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

            static std::shared_ptr<OutboundRelaySession> downcast(const std::shared_ptr<BaseSession>& b);

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
                std::string method, std::string body, bt_control_response_hook func) override;

            void build_more(size_t n = 0) override;

            void send_path_switch();

            void stop(bool send_close = false) override;

            void stop_session(bool send_close = false, bt_control_response_hook func = nullptr) override;
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
                intro_set cc,
                shared_kx_data kx_data);

            static std::shared_ptr<OutboundClientSession> downcast(const std::shared_ptr<BaseSession>& b);

          private:
            intro_path_map intro_path_mapping{};

            void populate_intro_map(const intro_set& intros);

            bool update_local_paths();

            void build_and_switch_paths();

            void build_and_switch_paths(intro_set intros);

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
                std::string method, std::string body, bt_control_response_hook func) override;

            void update_remote_intros(intro_set&& intros);

            void build_more(size_t n = 0) override;

            nlohmann::json ExtractStatus() const;

            void path_build_succeeded(std::shared_ptr<path::Path> p) override;

            void path_build_failed(std::shared_ptr<path::Path> p, bool timeout = false) override;

            void stop_session(bool send_close = false, bt_control_response_hook func = nullptr) override;

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

            static std::shared_ptr<InboundClientSession> downcast(const std::shared_ptr<BaseSession>& b);

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

            static std::shared_ptr<InboundRelaySession> downcast(const std::shared_ptr<BaseSession>& b);

            bool send_path_control_message(
                std::string method, std::string body, bt_control_response_hook func) override;
        };
    }  // namespace session

    namespace concepts
    {
        template <typename session_t>
        concept SessionType = std::is_base_of_v<llarp::session::BaseSession, session_t>;
    }  // namespace concepts
}  // namespace llarp
