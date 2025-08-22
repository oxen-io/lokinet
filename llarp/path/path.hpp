#pragma once

#include "transit_hop.hpp"

#include <llarp/constants/path.hpp>
#include <llarp/contact/client_contact.hpp>
#include <llarp/contact/tag.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/util/aligned.hpp>
#include <llarp/util/compare_ptr.hpp>
#include <llarp/util/thread/threading.hpp>
#include <llarp/util/time.hpp>

#include <chrono>
#include <functional>
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
        class PathHandler;

        /// Proxy object to produce a human readable hop list in log statements on demand.  This
        /// object is only intended to be used directly in format or log statements and not held.
        struct path_hop_stringifier
        {
            std::span<const TransitHop> hops;

            std::string to_string() const;
            static constexpr bool to_string_formattable = true;
        };

        struct Path final : public session_path_interface, public std::enable_shared_from_this<Path>
        {
            Path(
                Router& rtr,
                std::span<const RemoteRC> hop_rcs,
                PathHandler& handler,
                std::chrono::milliseconds expiry_ts);

            // hops on constructed path
            std::vector<TransitHop> hops;

            // If set, this is an aligned path to a pivot and this value is the hopid required to
            // send data through the pivot.
            std::optional<HopID> aligned_hopid;

            std::weak_ptr<PathHandler> handler;

            // Constructs a ClientInfo from this path, i.e. for including in a client contact.
            ClientIntro make_intro() const;

            nlohmann::json ExtractStatus() const;

            path_hop_stringifier hop_string() const;

            std::chrono::milliseconds LastRemoteActivityAt() const { return last_recv_msg; }

            void do_ping(std::chrono::milliseconds start_time);

            size_t num_hops() const { return hops.size(); }

            const std::chrono::milliseconds& expiry() const { return _expiry; }

            std::chrono::milliseconds expires_in(std::chrono::milliseconds now = llarp::time_now_ms()) const
            {
                return _expiry - now;
            }

            bool is_expired(std::chrono::milliseconds now = llarp::time_now_ms()) const { return _expiry < now; }

            void resolve_sns(
                std::span<const std::byte, SHORTHASHSIZE> name_hash, std::function<void(quic::message)> func);

            void fetch_relay_contact(const RouterID& needed, std::function<void(quic::message)> func);

            void fetch_relay_contacts(std::span<const RouterID> needed, std::function<void(quic::message)> func);

            void find_client_contact(const hash_key& location, std::function<void(quic::message)> func);

            void publish_client_contact(const EncryptedClientContact& ecc, std::function<void(quic::message)> func);

            void send_path_control_message(
                std::string_view method,
                std::span<const std::byte> body,
                std::function<void(quic::message)> func,
                std::byte type = std::byte{0x01}) override;

            void send_path_data_message(
                std::vector<std::byte>&& body,
                SymmNonce&& nonce = SymmNonce::make_random(),
                std::byte type = std::byte{0x01}) override;

            // The overhead added to encrypted path messages (either data messages or path control
            // messages) by the `encrypt_path_message` function.  This is the amount that the
            // `payload` needs to be extended to add encryption metadata, and so callers can use
            // this value to reserve the vector to be able to store the overhead without additional
            // allocations.
            inline static constexpr size_t ENCRYPT_PATH_MESSAGE_OVERHEAD = SymmNonce::SIZE + HopID::SIZE + 1;

            // Takes a payload and encrypts and extends it in-place to make it suitable for sending
            // down either the datagram channel (carrying traffic) or stream (carrying network
            // requests such as lookups or path builds).  This is *not* bt-encoded because we want
            // this to be as low overhead as possible for data messages, in particular.
            //
            // The given vector will be extended as part of this operation (to add nonce, hop,
            // packet type info).  To avoid a need for memory reallocation and copy, the caller
            // should optimally reserve enough space in the payload vector to ensure it has at least
            // ENCRYPT_PATH_MESSAGE_OVERHEAD additional bytes.
            //
            // nonce will be used if given, otherwise a random nonce is generated and used.  (It is
            // typically given when this is a session data message; see session.cpp).
            //
            // `type` must be a single byte, currently always equal to 0x01.  All other values are
            // reserved for future versions of the protocol that may need to change the fundamental
            // structure of encrypted data.
            void encrypt_path_message(
                std::vector<std::byte>& payload,
                SymmNonce&& nonce = SymmNonce::make_random(),
                std::byte type = std::byte{0x01});

            bool is_active(std::chrono::milliseconds now = llarp::time_now_ms()) const
            {
                return _is_established && !is_expired(now);
            }

            const TransitHop& edge() const { return hops.front(); }
            const TransitHop& terminus() const { return hops.back(); }

            std::string name() const;

            bool operator==(const Path& other) const;

            std::string to_string() const override;

            // The router ID at the end of the path.  For an outbound aligned path or inbound
            // session path, this is the pivot; for an outbound relay path this is the target relay.
            RouterID terminal_rid() const override { return terminus().router_id; }

            // The hop ID of this path used by remotes who want to reach us.  I.e. this is the pivot
            // hopid we publish in client intros, and is used for return traffic on established
            // outbound sessions.
            HopID terminal_hopid() const override { return terminus().txid; }

            void set_established();

            // Returns true if a path has been marked established.
            bool is_established() const { return _is_established; }

            // Marks a path as built.  This is primary used as a way to ensure we only build a Path
            // object once.  Returns true if the state was successfully changed (i.e. a false return
            // means the path was already built).
            bool set_built()
            {
                bool was_built = _is_built;
                _is_built = true;
                return not was_built;
            }

            // Returns true if a path has been marked as built.
            bool is_built() const { return _is_built; }

            struct ping_stats_printer {
                Path& p;
                std::string to_string() const;
                static constexpr bool to_string_formattable = true;
            };

            // Returns ping stats: response rate (0.0-1.0), and average (successful) ping response time
            ping_stats_printer printable_ping_stats() { return ping_stats_printer{*this}; }

          protected:
            /// call obtained exit hooks
            bool InformExitResult(std::chrono::milliseconds b);

            bool _is_built{false};
            bool _is_established{false};

            Router& _router;

            std::chrono::milliseconds _expiry{0s};
            std::chrono::milliseconds last_recv_msg{0s};
            std::chrono::milliseconds last_latency_test{0s};
            uint64_t last_latency_test_id{};

            static size_t next_path_log_id;
            const size_t path_log_id;  // Only used for log output

          private:
            int ping_responses{0}, ping_timeouts{0};
            int ping_recent_timeouts{0};
            // Cumulative time of all `ping_responses` pings (divide by ping_responses for an average).
            std::chrono::milliseconds ping_cumulative{0s};
            int64_t ping_sq_cumulative{0};
        };

    }  // namespace path
}  // namespace llarp

namespace std
{
    template <>
    struct hash<llarp::path::Path>
    {
        size_t operator()(const llarp::path::Path& p) const noexcept
        {
            return hash<llarp::HopID>{}(p.terminal_hopid()) ^ ((hash<llarp::HopID>{}(p.edge().rxid) << 13) >> 5);
        }
    };
}  //  namespace std
