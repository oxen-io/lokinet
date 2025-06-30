#pragma once

#include "router_id.hpp"

#include <llarp/constants/version.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/dns/srv_data.hpp>
#include <llarp/net/id.hpp>
#include <llarp/router_version.hpp>
#include <llarp/util/aligned.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/file.hpp>
#include <llarp/util/time.hpp>

#include <nlohmann/json.hpp>
#include <oxen/quic/address.hpp>
#include <oxenc/bt_producer.h>

namespace llarp
{
    namespace quic = oxen::quic;

    /** RelayContact
        On the wire we encode the data as a dict containing:
        - "" : the RC format version, which must be == RelayContact::VERSION for us to attempt to
                parse the reset of the fields.  (Future versions might have backwards-compat support
                for lower versions).
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
        - "~" : signature of all of the previous serialized data, signed by "p"
    */
    struct RelayContact
    {
        using time_point = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;

        static constexpr uint8_t VERSION{0};

        /// Unit tests disable this to allow private IP ranges in RCs, which normally get rejected.
        inline static bool BLOCK_BOGONS{true};

        /// Maximum permitted RC size.
        static constexpr size_t MAX_RC_SIZE{1024};

        /// How long (from its signing time) before an RC becomes "outdated".  Outdated records are
        /// used (e.g. for path building) only if there are no newer records available, such as
        /// might be the case when a client has been turned off for a while.
        static constexpr auto OUTDATED_AGE{12h};

        /// How long before an RC becomes invalid (and thus deleted).
        static constexpr auto LIFETIME{30 * 24h};

        std::string_view view() const { return _payload; }

        /// Getters for private attributes
        const quic::Address& addr() const { return _addr; }

        const std::optional<quic::Address>& addr6() const { return _addr6; }

        const RouterID& router_id() const { return _router_id; }

        const time_point& timestamp() const { return _timestamp; }

        NetID netid() const { return _netid; }

      protected:
        // advertised addresses
        quic::Address _addr;                  // refactor all 15 uses to use addr() method
        std::optional<quic::Address> _addr6;  // optional ipv6
        // public signing public key
        RouterID _router_id;

        time_point _timestamp{};
        NetID _netid = NetID::MAINNET;

        // Lokinet version at the time the RC was produced
        std::array<uint8_t, 3> _router_version;

        // In both Remote and Local RC's, the entire bt-encoded payload given at construction is
        // emplaced here.
        //
        //   In a RemoteRC, this value will be held for the lifetime of the object
        // s.t. it can be returned upon calls to ::bt_encode.
        //   In a LocalRC, this value will be supplanted any time a mutator is invoked, requiring
        // the re-signing of the payload.
        std::string _payload;

        // Loads data from the current `_payload` value.
        void load(NetID netid, bool accept_expired = false);

        auto compare_tuple() const { return std::tie(_router_id, _addr, _addr6, _timestamp, _router_version); }

      public:
        /// should we serialize the exit info?
        static const bool serializeExit = true;

        nlohmann::json extract_status() const;

        nlohmann::json to_json() const { return extract_status(); }

        std::string to_string() const;

        bool write(const fs::path& fname) const;

        bool operator==(const RelayContact& other) const { return compare_tuple() == other.compare_tuple(); }

        bool has_ip_overlap(const RelayContact& other, uint8_t netmask) const;

        /// does this RC expire soon? default delta is 1 minute
        bool expires_within_delta(std::chrono::milliseconds now, std::chrono::milliseconds dlt = 1min) const;

        /// returns true if this RC is outdated and should be fetched
        bool is_outdated(std::chrono::milliseconds now = llarp::time_now_ms()) const;

        /// returns true if this RC is expired and should be removed
        bool is_expired(std::chrono::milliseconds now) const;

        /// returns time in ms until we expire or 0 if we have expired
        std::chrono::milliseconds time_to_expiry(std::chrono::milliseconds now) const;

        /// get the age of this RC in ms
        std::chrono::milliseconds age(std::chrono::milliseconds now) const;

        bool other_is_newer(const RelayContact& other) const { return _timestamp < other._timestamp; }

        bool is_obsolete() const;

        static constexpr bool to_string_formattable = true;
    };

    struct RemoteRC;

    /// Extension of RelayContact used to store a local "RC," and inserts a RelayContact by
    /// re-parsing and sending it out. This sub-class contains a pubkey and all the other attributes
    /// required for signing and serialization
    struct LocalRC final : public RelayContact
    {
      private:
        std::array<std::byte, 64> _signature;
        Ed25519SecretKey _secret_key;

        void bt_sign_and_store(oxenc::bt_dict_producer&& btdp);

        oxenc::bt_dict_producer bt_encode_for_signing();

      public:
        LocalRC() = default;
        LocalRC(Ed25519SecretKey secret, quic::Address local, NetID netid);

        RemoteRC to_remote();

        void resign();

        auto operator==(const LocalRC& other) const
        {
            return RelayContact::operator==(other) && _signature == other._signature;
        }

        /// Mutators for the private member attributes. Calling on the mutators
        /// will clear the current signature and re-sign the RC
        void set_addr(quic::Address new_addr);
        void set_addr6(quic::Address new_addr);
        void clear_addr6();
        void set_router_id(RouterID rid);
    };

    /// Extension of RelayContact used in a "read-only" fashion. Parses the incoming RC to query
    /// the data in the constructor, eliminating the need for a ::verify method/
    struct RemoteRC final : public RelayContact
    {
      public:
        RemoteRC() = default;
        RemoteRC(std::string_view data, NetID netid, bool accept_expired = false);
        template <std::same_as<fs::path> FSPath>
        RemoteRC(const FSPath& fname, NetID netid, bool accept_expired = false);
    };
}  // namespace llarp

namespace std
{
    template <>
    struct hash<llarp::RelayContact>
    {
        virtual size_t operator()(const llarp::RelayContact& r) const noexcept
        {
            return std::hash<llarp::PubKey>{}(r.router_id());
        }
    };

    template <>
    struct hash<llarp::RemoteRC> : public hash<llarp::RelayContact>
    {};

    template <>
    struct hash<llarp::LocalRC> : public hash<llarp::RelayContact>
    {};
}  // namespace std
