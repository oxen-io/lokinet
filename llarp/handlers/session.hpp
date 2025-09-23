#pragma once

#include <llarp/address/address.hpp>
#include <llarp/config/config.hpp>
#include <llarp/contact/client_contact.hpp>
#include <llarp/path/path_handler.hpp>
#include <llarp/path/transit_hop.hpp>
#include <llarp/session/session.hpp>
#include <llarp/util/random.hpp>
#include <llarp/util/time.hpp>

#include <chrono>
#include <concepts>
#include <memory>

namespace llarp
{
    namespace rpc
    {
        class RPCServer;
    }

    namespace handlers
    {
        using session::session_tag;

        class SessionEndpoint final : public path::PathHandler
        {
            friend class rpc::RPCServer;
            friend class session::Session;

            std::unordered_set<dns::SRVData> _srv_records;

            // Inbound path lifetimes within a slot are always determined relative to this base
            // value, so that if we need a path in the (15,20] minute range, we will always pick the
            // same value in that slot by using this basis value.
            const std::chrono::seconds path_expiry_basis =
                std::chrono::floor<std::chrono::seconds>(llarp::time_now_ms());

            std::unordered_map<NetworkAddress, std::shared_ptr<session::Session>> _sessions;
            std::unordered_map<session_tag, std::shared_ptr<session::Session>> _session_tags;

            session_tag last_tag = llarp::csrng();

            // this could probably map to a pair of vectors, or pending packets could
            // be wrapped in callbacks, but for now this works
            // std::unordered_map<NetworkAddress, std::vector<IPPacket>> pending_sessions;
            // std::unordered_map<NetworkAddress, std::vector<std::function<void(bool)>>> pending_session_hooks;

            ClientContact client_contact;
            Ed25519BlindedKey cc_blind_keys;
            int cc_count = -1;
            protocol_flag protocols;

            // Used for logging connected/disconnected status:
            bool connected = false;

            // auth tokens for making outbound sessions; some of these are copied at construction,
            // some (with ONS names) get looked up and populated later.
            std::unordered_map<NetworkAddress, std::string> _auth_tokens;

            std::optional<std::string_view> fetch_auth_token(const NetworkAddress& remote) const;

            void close_session(std::shared_ptr<session::Session>& s, bool send_close);

            void on_path_build_failure(int64_t build_id, path::Path* path, bool timeout) override;
            void on_path_build_success(int64_t build_id, path::Path& p) override;

            void session_post_init(std::shared_ptr<session::Session> new_session);

          public:
            SessionEndpoint(Router& r);

            void stop(bool send_close);

            // Checks if we need more inbound paths and, if so, starts building them.
            void update_paths(std::chrono::milliseconds now) override;

            // bool build_path_to_random(bool exclude_current_termini)

            /// Returns array of:
            /// - inbound sessions (i.e. from remote clients)
            /// - outbound relay sessions (pending or established)
            /// - outbound client sessions (pending or established)
            /// - pending outbound relay sessions
            /// - pending outbound client sessions
            ///
            /// For relays, all but the first value will be 0 (relays do not establish outbound
            /// sessions).
            std::array<int, 5> session_stats() const;

            /// Returns array of path counts:
            /// - inbound/utility paths (used for inbound sessions and network queries)
            /// - paths for outbound relay sessions
            /// - paths for outbound client sessions
            std::array<int, 3> path_stats(std::chrono::milliseconds now = llarp::time_now_ms()) const;

            // quic::Address local_address() const { return _local_addr; }

            // get copy of all srv records
            std::unordered_set<dns::SRVData> srv_records() const { return _srv_records; }

            // Called when a relay receives a path switch (i.e. for an inbound relay session)
            bool recv_path_switch(
                const session_tag& t, const HopID& remote_pivot_txid, std::shared_ptr<path::TransitHop> new_thop);

            // Called when a client receives a path switch (i.e. for an inbound client session)
            bool recv_path_switch(const session_tag& t, const HopID& remote_pivot_txid, const HopID& local_pivot_txid);

            template <std::derived_from<session::Session> S = session::Session>
            S* get_session(const session_tag& tag) const
            {
                auto it = _session_tags.find(tag);
                if (it == _session_tags.end())
                    return nullptr;
                return dynamic_cast<S*>(it->second.get());
            }

            template <std::derived_from<session::Session> S = session::Session>
            S* get_session(const NetworkAddress& remote) const
            {
                auto it = _sessions.find(remote);
                if (it == _sessions.end())
                    return nullptr;
                return dynamic_cast<S*>(it->second.get());
            }

            bool close_session(NetworkAddress remote, bool send_close = false);

            bool close_session(session_tag t, bool send_close = false);

            /// Called to perform CC publishing.  This is called upon inbound path build completion
            /// if that completion results in a full set of target paths, so that we effectively
            /// republish whenever inbound paths change.
            void update_and_publish_localcc();

            void publish_client_contact(const EncryptedClientContact& ecc);

            // SessionEndpoint can use either a whitelist or a static auth token list to  validate incomininbg requests
            // to initiate a session
            bool validate(const NetworkAddress& remote, std::optional<std::string> maybe_auth = std::nullopt);

            // FIXME: should SessionEndpoint have these mappings at all?
            std::optional<std::variant<ipv4, ipv6>> map_session(const session::Session& s);
            void map_remote_to_local_addr(NetworkAddress remote, quic::Address local);
            void unmap_local_addr_by_remote(const NetworkAddress& remote);
            void unmap_remote_by_name(const std::string& name);

            void handle_session_init(std::vector<std::byte>&& payload, std::shared_ptr<path::Path> path);
            void handle_session_init(std::vector<std::byte>&& payload, std::shared_ptr<path::TransitHop> thop);

            // Called on a client when we receive a session_init from another client to create an
            // InboundClientSession.  Returns nullopt if the session cannot be created, otherwise
            // returns the random session tag we have associated with the inbound session.
            std::optional<session_tag> create_inbound_session(
                const NetworkAddress& initiator,
                const HopID& remote_pivot_txid,
                std::shared_ptr<path::Path> path,
                const SharedSecret& session_key);

            // Called on a relay when we receive a session_init from a client to create an
            // InboundRelaySession.  Returns nullopt if the session cannot be created, otherwise
            // returns the random session tag we have associated with the inbound session.
            std::optional<session_tag> create_inbound_session(
                const NetworkAddress& initiator,
                const HopID& remote_pivot_txid,
                std::shared_ptr<path::TransitHop> path,
                const SharedSecret& session_key);

            // lookup SNS address to return "{pubkey}.loki" hidden service or exit node operated on a remote client
            void resolve_sns(std::string name, std::function<void(std::optional<NetworkAddress>)> func);

            void lookup_remote_srv(
                std::string name, std::string service, std::function<void(std::vector<dns::SRVData>)> handler);

            void lookup_relay_contact(RouterID remote, std::function<void(std::optional<RelayContact>)> func);

            void lookup_client_intro(RouterID remote, std::function<void(std::optional<ClientContact>)> func);

            // resolves any config mappings that parsed ONS addresses to their pubkey network address
            void resolve_sns_mappings();

            // Initiates a session to the given remote client or snode address.  Calls
            // `on_attempted` when the connection is either established (immediately, if a session
            // to the target is already established) or when the connection attempt times out (the
            // caller can check `session.is_established()` to figure out which one occured).
            //
            // The timeout, if omitted/nullopt, defaults to the [paths]build-timeout config option.
            //
            // Note that this resulting session could be outbound or inbound: i.e. if the target is
            // a client (.loki) that has already established a session to this lokinet instance then
            // that existing session is used rather than building a new outbound one.
            //
            // NB: this method can be safely called from outside the event loop (e.g. in embedded
            // usage).
            std::shared_ptr<session::Session> initiate_remote_session(
                const NetworkAddress& remote,
                std::function<void(session::Session& session)> on_established,
                std::optional<std::chrono::milliseconds> timeout = std::nullopt);

            // More internal version of initiate_remote_session: this may only be called from inside
            // the router loop, takes no callback, and returns the Session (which may be brand new
            // if one did not already exist to the remote).
            std::shared_ptr<session::Session> remote_session(const NetworkAddress& remote);

            void tick(std::chrono::milliseconds now) override;

            void queue_session_packet(const NetworkAddress& remote, IPPacket pkt);

            session_tag next_tag();
        };

    }  // namespace handlers
}  //  namespace llarp
