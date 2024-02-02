#pragma once

#include "abstracthophandler.hpp"
#include "path_types.hpp"
#include "pathset.hpp"

#include <llarp/constants/path.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/router_id.hpp>
#include <llarp/service/intro.hpp>
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
    struct Router;

    namespace path
    {
        struct TransitHop;
        struct TransitHopInfo;
        struct PathHopConfig;

        struct Endpoint_Hash;
        struct endpoint_comparator;

        /// A path we made
        struct Path final : public AbstractHopHandler, public std::enable_shared_from_this<Path>
        {
            std::vector<PathHopConfig> hops;

            std::weak_ptr<PathSet> path_set;

            service::Introduction intro;

            llarp_time_t buildStarted = 0s;

            Path(
                Router* rtr,
                const std::vector<RemoteRC>& routers,
                std::weak_ptr<PathSet> parent,
                PathRole startingRoles,
                std::string shortName);

            util::StatusObject ExtractStatus() const;

            PathRole Role() const
            {
                return _role;
            }

            bool operator<(const Path& other) const
            {
                return hops < other.hops;
            }

            void MarkActive(llarp_time_t now)
            {
                last_recv_msg = std::max(now, last_recv_msg);
            }

            /// return true if ALL of the specified roles are supported
            bool SupportsAllRoles(PathRole roles) const
            {
                return (_role & roles) == roles;
            }

            /// return true if ANY of the specified roles are supported
            bool SupportsAnyRoles(PathRole roles) const
            {
                return roles == ePathRoleAny || (_role | roles) != 0;
            }

            /// clear role bits
            void ClearRoles(PathRole roles)
            {
                _role &= ~roles;
            }

            PathStatus Status() const
            {
                return _status;
            }

            const std::string& short_name() const;

            std::string HopsString() const;

            llarp_time_t LastRemoteActivityAt() const override
            {
                return last_recv_msg;
            }

            void EnterState(PathStatus st, llarp_time_t now = 0s);

            llarp_time_t ExpireTime() const
            {
                return buildStarted + hops[0].lifetime;
            }

            bool ExpiresSoon(llarp_time_t now, llarp_time_t dlt = 5s) const override
            {
                return now >= (ExpireTime() - dlt);
            }

            void enable_exit_traffic()
            {
                log::info(path_cat, "{} {} granted exit", name(), Endpoint());
                _role |= ePathRoleExit;
            }

            void mark_exit_closed()
            {
                log::info(path_cat, "{} hd its exit closed", name());
                _role &= ePathRoleExit;
            }

            bool update_exit(uint64_t tx_id);

            bool Expired(llarp_time_t now) const override;

            /// build a new path on the same set of hops as us
            /// regenerates keys
            void Rebuild();

            void Tick(llarp_time_t now, Router* r);

            bool find_name(std::string name, std::function<void(std::string)> func = nullptr);

            bool find_intro(
                const dht::Key_t& location,
                bool is_relayed = false,
                uint64_t order = 0,
                std::function<void(std::string)> func = nullptr);

            bool close_exit(SecretKey sk, std::string tx_id, std::function<void(std::string)> func = nullptr);

            bool obtain_exit(
                SecretKey sk, uint64_t flag, std::string tx_id, std::function<void(std::string)> func = nullptr);

            /// sends a control request along a path
            ///
            /// performs the necessary onion encryption before sending.
            /// func will be called when a timeout occurs or a response is received.
            /// if a response is received, onion decryption is performed before func is called.
            ///
            /// func is called with a bt-encoded response string (if applicable), and
            /// a timeout flag (if set, response string will be empty)
            bool send_path_control_message(
                std::string method, std::string body, std::function<void(std::string)> func = nullptr) override;

            bool IsReady() const;

            // Is this deprecated?
            // nope not deprecated :^DDDD
            PathID_t TXID() const;

            RouterID Endpoint() const;

            PubKey EndpointPubKey() const;

            bool is_endpoint(const RouterID& router, const PathID_t& path) const;

            PathID_t RXID() const override;

            RouterID upstream() const;

            std::string name() const;

           private:
            bool SendLatencyMessage(Router* r);

            /// call obtained exit hooks
            bool InformExitResult(llarp_time_t b);

            Router& router;
            llarp_time_t last_recv_msg = 0s;
            llarp_time_t last_latency_test = 0s;
            uint64_t last_latency_test_id = 0;
            PathStatus _status;
            PathRole _role;
            const std::string _short_name;
        };

        struct Hash
        {
            size_t operator()(const Path& p) const
            {
                const auto& tx = p.hops[0].txID;
                const auto& rx = p.hops[0].rxID;
                const auto& r = p.hops[0].upstream;
                const size_t rhash = std::accumulate(r.begin(), r.end(), 0, std::bit_xor{});
                return std::accumulate(
                    rx.begin(),
                    rx.begin(),
                    std::accumulate(tx.begin(), tx.end(), rhash, std::bit_xor{}),
                    std::bit_xor{});
            }
        };

        /// hash for std::shared_ptr<Path> by path endpoint
        struct Endpoint_Hash
        {
            size_t operator()(const std::shared_ptr<Path>& p) const
            {
                if (p == nullptr)
                    return 0;
                return std::hash<RouterID>{}(p->Endpoint());
            }
        };

        /// comparision for equal endpoints
        struct endpoint_comparator
        {
            bool operator()(const std::shared_ptr<Path>& left, const std::shared_ptr<Path>& right) const
            {
                return left && right && left->Endpoint() == left->Endpoint();
            }
        };
    }  // namespace path
}  // namespace llarp
