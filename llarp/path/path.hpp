#pragma once

#include "path_handler.hpp"
#include "transit_hop.hpp"

#include <llarp/constants/path.hpp>
#include <llarp/contact/client_contact.hpp>
#include <llarp/contact/tag.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/util/aligned.hpp>
#include <llarp/util/compare_ptr.hpp>
#include <llarp/util/thread/threading.hpp>
#include <llarp/util/time.hpp>

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace llarp
{
    class Router;
    struct Profiling;

    namespace service
    {
        struct EncryptedIntroSet;
    }

    namespace path
    {
        /// A path we made
        struct Path final : public session_path_interface, public std::enable_shared_from_this<Path>
        {
            friend struct PathHandler;
            friend class handlers::SessionEndpoint;
            friend struct llarp::Profiling;
            friend struct LinkManager;

            Path(Router& rtr, const std::vector<RemoteRC>& routers, std::weak_ptr<PathHandler> parent);

            ~Path();

            // hops on constructed path
            std::vector<TransitHop> hops;

            std::weak_ptr<PathHandler> handler;
            ClientIntro intro{};

            std::shared_ptr<Path> get_self() { return shared_from_this(); }

            std::weak_ptr<Path> get_weak() { return weak_from_this(); }

            nlohmann::json ExtractStatus() const;

            std::string hop_string() const;

            std::chrono::milliseconds LastRemoteActivityAt() const { return last_recv_msg; }

            void do_ping(std::chrono::milliseconds start_time);

            void link_session(session_tag t) override;

            bool unlink_session(session_tag t) override;

            bool is_linked() const override { return not _linked_sessions.empty(); }

            size_t num_links() const { return _linked_sessions.size(); }

            bool is_expired(std::chrono::milliseconds now = llarp::time_now_ms()) const;

            void Tick(std::chrono::milliseconds now);

            bool resolve_sns(const std::string& name_hash, std::function<void(quic::message)> func);

            bool fetch_relay_contact(const RouterID& needed, std::function<void(quic::message)> func);

            bool find_client_contact(const hash_key& location, std::function<void(quic::message)> func);

            bool publish_client_contact(const EncryptedClientContact& ecc, std::function<void(quic::message)> func);

            bool send_path_control_message(
                std::string method, std::string body, std::function<void(quic::message)> func) override;

            bool send_path_data_message(std::string body) override;

            std::string make_path_message(std::string payload);

            std::string make_path_data_message(std::string payload);

            bool is_active(std::chrono::milliseconds now = llarp::time_now_ms()) const;

            const TransitHop& edge() const { return hops.front(); }
            const TransitHop& pivot() const { return hops.back(); }

            std::string name() const;

            bool operator==(const Path& other) const;

            std::string to_string() const override;
            static constexpr bool to_string_formattable = true;

            // TESTNET: debug
            std::string debug_string() const;

            RouterID terminal_rid() const override { return pivot().router_id(); }

            HopID terminal_txid() const override { return pivot().txid(); }

          protected:
            // Called by SessionEndpoint to indicate the path is successfully built
            void set_established();

            // Called by SessionEndpoint to check path status for internal management, and is made protected. All
            // other objects are more concerned with ::is_active() and ::is_linked(), which include expiry status
            // and session activity
            bool is_established() const { return _is_established; }

            void populate_internals(const std::vector<RemoteRC>& _hops);

            /// call obtained exit hooks
            bool InformExitResult(std::chrono::milliseconds b);

            bool _is_established{false};

            Router& _router;

            const size_t num_hops;

            std::unordered_set<session_tag> _linked_sessions;

            std::chrono::milliseconds last_recv_msg{0s};
            std::chrono::milliseconds last_latency_test{0s};
            uint64_t last_latency_test_id{};

            // TESTNET: debug
            static size_t next_path_uuid;
            const size_t path_id;

          private:
            uint64_t ping_count{0};
            uint64_t recent_ping_failures{0};
            std::chrono::milliseconds ping_average{0s};
        };

        struct PathExpComp
        {
            bool operator()(const std::shared_ptr<Path>& lhs, const std::shared_ptr<Path>& rhs) const
            {
                return lhs->intro.expiry > rhs->intro.expiry;
            }
        };

        using PathPtrSet = std::set<std::shared_ptr<Path>, PathExpComp>;

    }  // namespace path
}  // namespace llarp

namespace std
{
    template <>
    struct hash<llarp::path::Path>
    {
        size_t operator()(const llarp::path::Path& p) const noexcept
        {
            return hash<llarp::HopID>{}(p.pivot().txid()) ^ ((hash<llarp::HopID>{}(p.edge().rxid()) << 13) >> 5);
        }
    };
}  //  namespace std
