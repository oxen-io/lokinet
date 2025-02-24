#pragma once

#include <llarp/address/map.hpp>
#include <llarp/config/config.hpp>
#include <llarp/contact/client_contact.hpp>
#include <llarp/path/path_handler.hpp>
#include <llarp/session/map.hpp>

namespace llarp
{
    inline constexpr size_t NUM_SESSION_PATHS{4};

    class EventLoop;

    namespace rpc
    {
        class RPCServer;
    }

    namespace handlers
    {
        class SessionEndpoint final : public path::PathHandler
        {
            friend class rpc::RPCServer;
            friend struct session::BaseSession;

            const bool _is_exit_node{false};
            const bool _is_service_node{false};

            std::unordered_set<dns::SRVData> _srv_records;

            bool should_publish_cc{false};

            session_map<NetworkAddress, session::BaseSession> _sessions;

            address_map<oxen::quic::Address, NetworkAddress> _address_map;

            // Remote client exit-node addresses mapped to local IP ranges
            //  - Directly pre-loaded from config
            address_map<IPRange, NetworkAddress> _range_map;

            ClientContact client_contact;
            uint8_t protoflags;

            std::shared_ptr<EventTicker> _cc_publisher;

            // auth tokens for making outbound sessions
            std::unordered_map<NetworkAddress, std::string> _auth_tokens;

            // static auth tokens for authenticating inbound sessions
            std::unordered_set<std::string> _static_auth_tokens;
            // whitelist for authenticating inbound sessions
            std::unordered_set<NetworkAddress> _auth_whitelist;

            bool _use_tokens{false};
            bool _use_whitelist{false};

            IPRange _local_range;
            oxen::quic::Address _local_addr;
            ip_v _local_base_ip;
            ip_v _next_ip;
            std::string _if_name;

            bool _ipv6_enabled{};

            std::optional<std::string_view> fetch_auth_token(const NetworkAddress& remote) const;

            // Ranges reachable via our endpoint -- Exit mode only!
            std::set<IPRange> _routed_ranges;  // formerly from LocalEndpoint

            // policies about traffic that we are willing to carry -- Exit mode only!
            std::optional<net::ExitPolicy> _exit_policy = std::nullopt;

            void unmap_session(NetworkAddress remote, bool using_tun = true);

            void close_session(std::shared_ptr<session::BaseSession>& s, bool send_close);

          protected:
            void rotate_paths() override;

            void drop_oldest_path() override;

            void path_rotation_succeeded(std::shared_ptr<path::Path> new_path) override;

            std::optional<std::vector<RemoteRC>> get_hops_to_random() override;

          public:
            SessionEndpoint(Router& r);

            void configure();

            void stop(bool send_close = false) override;

            void build_more(size_t n = 0) override;

            const std::shared_ptr<EventLoop>& loop();

            std::tuple<size_t, std::string, bool> session_stats() const;

            std::shared_ptr<path::PathHandler> get_self() override { return shared_from_this(); }

            std::weak_ptr<path::PathHandler> get_weak() override { return weak_from_this(); }

            bool is_exit_node() const { return _is_exit_node; }

            bool is_service_node() const { return _is_service_node; }

            oxen::quic::Address local_address() const { return _local_addr; }

            // get copy of all srv records
            std::set<dns::SRVData> srv_records() const { return {_srv_records.begin(), _srv_records.end()}; }

            bool recv_path_switch(session_tag t, HopID remove_pivot, HopID local_pivot);

            template <concepts::SessionType session_t = session::BaseSession>
            std::shared_ptr<session_t> get_session(const session_tag& tag) const
            {
                return std::static_pointer_cast<session_t>(_sessions.get_session(tag));
            }

            template <concepts::SessionType session_t = session::BaseSession>
            std::shared_ptr<session_t> get_session(const NetworkAddress& remote) const
            {
                return std::static_pointer_cast<session_t>(_sessions.get_session(remote));
            }

            bool close_session(NetworkAddress remote, bool send_close = false);

            bool close_session(session_tag t, bool send_close = false);

            void srv_records_changed();

            // This function can be called with the fields to be updated. ClientIntros are always passed, so there
            // is no need to pass them to this function
            template <typename... Opt>
            void update_and_publish_localcc(Opt&&... args)
            {
                auto intros = get_local_client_intros();
                if (intros.empty())
                    return _localcc_update_fail();
                client_contact.regenerate(std::move(intros), std::forward<Opt>(args)...);
                _update_and_publish_localcc();
            }

            void update_and_publish_localcc();

            void start_tickers();

            bool publish_client_contact(const EncryptedClientContact& ecc);

            // SessionEndpoint can use either a whitelist or a static auth token list to  validate incomininbg requests
            // to initiate a session
            bool validate(const NetworkAddress& remote, std::optional<std::string> maybe_auth = std::nullopt);

            std::optional<session_tag> prefigure_session(
                NetworkAddress initiator,
                // session_tag tag,
                HopID remote_pivot_txid,
                std::shared_ptr<path::Path> path,
                std::optional<shared_kx_data> kx_data,
                bool use_tun);

            // lookup SNS address to return "{pubkey}.loki" hidden service or exit node operated on a remote client
            void resolve_ons(std::string name, std::function<void(std::optional<NetworkAddress>)> func = nullptr);

            void lookup_remote_srv(
                std::string name, std::string service, std::function<void(std::vector<dns::SRVData>)> handler);

            void lookup_relay_contact(RouterID remote, std::function<void(std::optional<RemoteRC>)> func);

            void lookup_client_intro(RouterID remote, std::function<void(std::optional<ClientContact>)> func);

            // resolves any config mappings that parsed ONS addresses to their pubkey network address
            void resolve_ons_mappings();

            void initiate_remote_session(const NetworkAddress& remote, on_session_init_hook cb);

            void tick(std::chrono::milliseconds now) override;

            // TESTNET: the following functions may not be needed -- revisit this
            /*  Address Mapping - Public Mutators  */
            void map_remote_to_local_addr(NetworkAddress remote, oxen::quic::Address local);

            void unmap_local_addr_by_remote(const NetworkAddress& remote);

            void unmap_remote_by_name(const std::string& name);

            /*  IPRange Mapping - Public Mutators  */
            void map_remote_to_local_range(NetworkAddress remote, IPRange range);

            void unmap_local_range_by_remote(const NetworkAddress& remote);

            void unmap_range_by_name(const std::string& name);

          private:
            void _localcc_update_fail();

            void _update_and_publish_localcc();

            void _initiate_client_session(NetworkAddress remote, on_session_init_hook cb);

            void _initiate_relay_session(NetworkAddress remote, on_session_init_hook cb);

            void _make_client_session_path(intro_set intros, NetworkAddress remote, on_session_init_hook cb);

            void _make_relay_session_path(RemoteRC rc, NetworkAddress remote, on_session_init_hook cb);

            void _make_client_session(
                intro_set remote_intros,
                NetworkAddress remote,
                ClientIntro remote_intro,
                std::shared_ptr<path::Path> path,
                on_session_init_hook cb);

            void _make_relay_session(
                RemoteRC rc, NetworkAddress remote, std::shared_ptr<path::Path> path, on_session_init_hook cb);
        };

    }  // namespace handlers
}  //  namespace llarp
