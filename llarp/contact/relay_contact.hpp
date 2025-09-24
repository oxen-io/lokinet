#pragma once

#include "router_id.hpp"

#include <llarp/net/id.hpp>
#include <llarp/util/time.hpp>

#include <nlohmann/json_fwd.hpp>
#include <oxen/quic/address.hpp>
#include <oxenc/bt_producer.h>

#include <filesystem>

namespace llarp
{
    class Router;

    namespace quic = oxen::quic;

    /** RelayContact
        On the wire we encode the data as a dict containing:
        - "" : the RC format version, omitted when 0, and which must be == RelayContact::VERSION for
               us to attempt to parse the reset of the fields.  (Future versions might have
               backwards-compat support for lower versions).
        - "4" : 6 byte packed IPv4 address & port: 4 bytes of IPv4 address followed by 2 bytes of
                port, both encoded in network (i.e. big-endian) order.
        - "6" : optional 18 byte IPv6 address & port: 16 byte raw IPv6 address followed by 2 bytes
                of port in network order.
        - "i" : optional network ID integer: 0 or omitted for mainnet, 1 for testnet.
        - "p" : 32-byte router pubkey (Ed25519)
        - "t" : timestamp when this RC record was created (which also implicitly determines when it
                goes stale and when it expires).
        - "v" : lokinet version of the router; this is a three-byte packed value of
                MAJOR, MINOR, PATCH, e.g. \x00\x0a\x03 for 0.10.3.
        - "~" : signature of all of the previous serialized data, signed by "p", and *must* be the
                last item in the dict.
    */
    struct RelayContact
    {
        /// The RC version.  Changing this means the RC will not be accepted by any previous
        /// versions of Lokinet.
        static constexpr uint8_t VERSION{0};

        /// Unit tests disable this to allow private IP ranges in RCs, which normally get rejected.
        inline static bool BLOCK_BOGONS{true};

        /// Maximum permitted RC size.  This is considerably larger than needed to allow future
        /// versions to add various fields without breaking the ability for existing lokinet
        /// versions to handle the RC (for example: a ML-KEM-1024 PQC key is 1568 bytes).
        static constexpr size_t MAX_RC_SIZE{2048};

        /// How long (from its signing time) before an RC becomes "outdated".  Outdated records are
        /// used (e.g. for path building) only if there are no newer records available, such as
        /// might be the case when a client has been turned off for a while.
        static constexpr auto OUTDATED_AGE{12h};

        /// How long before an RC becomes invalid (and thus deleted).
        static constexpr auto LIFETIME{30 * 24h};

        /// Minimum age difference between an existing RC and a new, gossipped RC from the same
        /// relay.  We ignore RCs that are not more than this amount older than the current one.
        static constexpr auto MIN_GOSSIP_RC_AGE = 1min;

        std::string_view view() const { return _payload; }

        /// Getters for private attributes
        const quic::Address& addr() const { return _addr; }

        const std::optional<quic::Address>& addr6() const { return _addr6; }

        const RouterID& router_id() const { return _router_id; }

        const std::chrono::sys_seconds& timestamp() const { return _timestamp; }

        NetID netid() const { return _netid; }

      private:
        // public signing public key
        RouterID _router_id;

        // advertised addresses
        quic::Address _addr;
        std::optional<quic::Address> _addr6;  // optional ipv6

        std::chrono::sys_seconds _timestamp{};
        NetID _netid = NetID::MAINNET;

        // Lokinet version at the time the RC was produced
        std::array<uint8_t, 3> _router_version;

        // Contains the full bt-encoded payload of the RC.
        std::string _payload;

        // Loads data from the current `_payload` value.
        void load(NetID netid, bool accept_expired = false);

        auto compare_tuple() const { return std::tie(_router_id, _addr, _addr6, _timestamp, _router_version); }

      public:
        RelayContact() = default;
        // Parses a serialized RC
        RelayContact(std::string_view data, NetID netid, bool accept_expired = false);
        // Reads a serialized RC from disk and parses it
        template <std::same_as<std::filesystem::path> FSPath>
        RelayContact(const FSPath& fname, NetID netid, bool accept_expired = false);

        // Constructs a signed RC from the info in the given Router object.
        explicit RelayContact(const Router& router);

        nlohmann::json extract_status() const;

        bool write(const std::filesystem::path& fname) const;

        bool operator==(const RelayContact& other) const { return compare_tuple() == other.compare_tuple(); }

        bool has_ip_overlap(const RelayContact& other, uint8_t netmask) const;

        /// does this RC expire soon? default delta is 1 minute
        bool expires_within_delta(std::chrono::milliseconds now, std::chrono::milliseconds dlt = 1min) const;

        /// returns true if this RC is outdated and should be re-fetched
        bool is_outdated(std::chrono::milliseconds now = llarp::time_now_ms()) const;

        /// returns true if this RC is expired and should be removed
        bool is_expired(std::chrono::milliseconds now) const;

        /// returns time in ms until we expire or 0 if we have expired
        std::chrono::milliseconds time_to_expiry(std::chrono::milliseconds now) const;

        /// get the age of this RC in ms
        std::chrono::milliseconds age(std::chrono::milliseconds now) const;

        // Returns true if this RC is at least `at_least` newer than `other`.  (By default threshold
        // is 1s, which is the minimum precision of RCs, and so this returns true if this is at all
        // newer than other).
        bool newer_than(const RelayContact& other, std::chrono::seconds at_least = 1s) const
        {
            return _timestamp - other._timestamp >= at_least;
        }

        // Returns true if this RC has a different contact address (IP/port) from `other`.  This is
        // used when deciding how important an RC update is when deciding whether to gossip (minor
        // updates are only gossipped if they change this contact info).
        bool address_changed(const RelayContact& other) const;

        // Returns true if this RC is on the hard-coded list of obsolete bootstrap nodes; this is
        // only used when loading bootstraps to ensure known, no-longer-valid bootstraps are
        // excluded even if in a stale bootstrap.signed file.
        bool is_obsolete() const;

        std::string to_string() const;
        static constexpr bool to_string_formattable = true;
    };

}  // namespace llarp

template <>
struct std::hash<llarp::RelayContact>
{
    virtual size_t operator()(const llarp::RelayContact& r) const noexcept
    {
        return std::hash<llarp::PubKey>{}(r.router_id());
    }
};
