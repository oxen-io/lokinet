#pragma once

#include <llarp/address/address.hpp>
#include <llarp/address/map.hpp>
#include <llarp/config/config.hpp>
#include <llarp/contact/client_contact.hpp>
#include <llarp/path/path_handler.hpp>
#include <llarp/session/map.hpp>
#include <llarp/session/session.hpp>

#include <concepts>
#include <memory>

namespace llarp
{
    inline constexpr size_t NUM_SESSION_PATHS{4};

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

            std::unordered_set<dns::SRVData> _srv_records;

            bool should_publish_cc{false};

            session_map _sessions;

            address_map<quic::Address> _address_map;

            // this could probably map to a pair of vectors, or pending packets could
            // be wrapped in callbacks, but for now this works
            std::unordered_map<NetworkAddress, std::vector<IPPacket>> pending_sessions;
            std::unordered_map<NetworkAddress, std::vector<std::function<void(bool)>>> pending_session_hooks;

            ClientContact client_contact;
            protocol_flag protocols;

            std::shared_ptr<quic::Ticker> _cc_publisher;

            // auth tokens for making outbound sessions; some of these are copied at construction,
            // some (with ONS names) get looked up and populated later.
            std::unordered_map<NetworkAddress, std::string> _auth_tokens;

            std::optional<std::string_view> fetch_auth_token(const NetworkAddress& remote) const;

            void unmap_session(NetworkAddress remote, bool using_tun = true);

            void close_session(std::shared_ptr<session::BaseSession>& s, bool send_close);

          protected:
            void rotate_paths() override;

            void drop_oldest_path() override;

            void path_rotation_succeeded(const std::shared_ptr<path::Path>& new_path) override;

            std::optional<std::vector<RemoteRC>> get_hops_to_random() override;

          public:
            SessionEndpoint(Router& r);

            void stop(bool send_close = false) override;

            void build_more(size_t n = 0) override;

            const std::shared_ptr<quic::Loop>& loop();

            std::pair<size_t, bool> session_stats() const;

            std::shared_ptr<path::PathHandler> get_self() override { return shared_from_this(); }

            std::weak_ptr<path::PathHandler> get_weak() override { return weak_from_this(); }

            // quic::Address local_address() const { return _local_addr; }

            // get copy of all srv records
            std::unordered_set<dns::SRVData> srv_records() const { return _srv_records; }

            bool recv_path_switch(
                session_tag t, HopID remote_pivot_txid, std::shared_ptr<session_path_interface> new_path);

            bool recv_path_switch(session_tag t, HopID remote_pivot_txid, HopID local_pivot_txid);

            template <std::derived_from<session::BaseSession> Session = session::BaseSession>
            std::shared_ptr<Session> get_session(const session_tag& tag) const
            {
                auto s = _sessions.get_session(tag);
                if constexpr (!std::same_as<Session, session::BaseSession>)
                    return std::dynamic_pointer_cast<Session>(std::move(s));
                else
                    return s;
            }

            template <std::derived_from<session::BaseSession> Session = session::BaseSession>
            std::shared_ptr<Session> get_session(const NetworkAddress& remote) const
            {
                auto s = _sessions.get_session(remote);
                if constexpr (!std::same_as<Session, session::BaseSession>)
                    return std::dynamic_pointer_cast<Session>(std::move(s));
                else
                    return s;
            }

            bool close_session(NetworkAddress remote, bool send_close = false);

            bool close_session(session_tag t, bool send_close = false);

            void update_and_publish_localcc();

            void start_tickers();

            bool publish_client_contact(const EncryptedClientContact& ecc);

            // SessionEndpoint can use either a whitelist or a static auth token list to  validate incomininbg requests
            // to initiate a session
            bool validate(const NetworkAddress& remote, std::optional<std::string> maybe_auth = std::nullopt);

            std::optional<std::variant<ipv4, ipv6>> map_session(const session::BaseSession& s);

            std::optional<session_tag> prefigure_session(
                NetworkAddress initiator,
                HopID remote_pivot_txid,
                std::shared_ptr<session_path_interface> path,
                shared_kx_data kx_data,
                bool use_tun);

            // lookup SNS address to return "{pubkey}.loki" hidden service or exit node operated on a remote client
            void resolve_sns(std::string name, std::function<void(std::optional<NetworkAddress>)> func);

            void lookup_remote_srv(
                std::string name, std::string service, std::function<void(std::vector<dns::SRVData>)> handler);

            void lookup_relay_contact(RouterID remote, std::function<void(std::optional<RemoteRC>)> func);

            void lookup_client_intro(RouterID remote, std::function<void(std::optional<ClientContact>)> func);

            // resolves any config mappings that parsed ONS addresses to their pubkey network address
            void resolve_sns_mappings();

            // NB: this method can be called from outside the event loop (e.g. in embedded usage).
            void initiate_remote_session(NetworkAddress remote, on_session_init_hook cb);

            void tick(std::chrono::milliseconds now) override;

            // TESTNET: the following functions may not be needed -- revisit this
            /*  Address Mapping - Public Mutators  */
            void map_remote_to_local_addr(NetworkAddress remote, quic::Address local);

            void unmap_local_addr_by_remote(const NetworkAddress& remote);

            void unmap_remote_by_name(const std::string& name);

            /*  IPRange Mapping - Public Mutators  */
            // TODO FIXME: these need fixing as they currently erroneously assume a 1-1 relationship
            // between exit ranges and exit addresses.
            // void map_remote_to_local_range(NetworkAddress remote, IPRange range);
            // void unmap_local_range_by_remote(const NetworkAddress& remote);
            // void unmap_range_by_name(const std::string& name);

            bool have_pending_session(const NetworkAddress& remote);

            void queue_session_packet(const NetworkAddress& remote, IPPacket pkt);

          private:
            void _localcc_update_fail();

            void _update_and_publish_localcc();

            void _initiate_client_session(NetworkAddress remote, on_session_init_hook cb);

            void _initiate_relay_session(NetworkAddress remote, on_session_init_hook cb);

            void _make_client_session_path(sorted_intro_set intros, NetworkAddress remote, on_session_init_hook cb);

            void _make_relay_session_path(RemoteRC rc, NetworkAddress remote, on_session_init_hook cb);

            void _make_client_session(
                sorted_intro_set remote_intros,
                NetworkAddress remote,
                ClientIntro remote_intro,
                std::shared_ptr<path::Path> path,
                on_session_init_hook cb);

            void _make_relay_session(
                RemoteRC rc, NetworkAddress remote, std::shared_ptr<path::Path> path, on_session_init_hook cb);
        };

    }  // namespace handlers
}  //  namespace llarp
