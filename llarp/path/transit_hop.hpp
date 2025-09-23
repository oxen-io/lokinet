#pragma once

#include "hopid.hpp"

#include <llarp/constants/path.hpp>
#include <llarp/contact/router_id.hpp>
#include <llarp/util/aligned.hpp>
#include <llarp/util/compare_ptr.hpp>

#include <oxen/quic/connection_ids.hpp>

namespace llarp
{
    class Router;
}  // namespace llarp

namespace llarp::path
{
    class TransitHopError : public std::runtime_error
    {
      public:
        std::string error_code;
        TransitHopError(std::string err_code);

        /// Pre-defined error codes:
        inline static TransitHopError INVALID_DATA() { return "INVALID DATA"s; }
        inline static TransitHopError DH_PUBKEY() { return "INVALID DH PUBKEY"s; }
        inline static TransitHopError INVALID_PAYLOAD() { return "INVALID TRANSIT HOP PAYLOAD"s; }
        inline static TransitHopError INVALID_HOP_ID() { return "INVALID TRANSIT HOP IDS"s; }
        inline static TransitHopError HOP_ID_UNAVAILABLE() { return "TRANSIT HOP ID ALREADY IN USE"s; }
        inline static TransitHopError INVALID_LIFETIME() { return "INVALID PATH LIFETIME"s; }
    };

    // TransitHop holds the raw data associated with a single hop in a path, e.g. hop ids, keys,
    // expiry, and so on.  It is primarily just a container to hold this data, and lives at the
    // relevant hop on the relay.
    struct TransitHop
    {
        HopID txid, rxid;

        // Along a path "upstream" is the next router away from the client, "downstream" is the hop
        // towards the client (or the connection itself, at the edge).  The pivot (which has no
        // upstream) is identified by the upstream value being equal to itself.
        //
        // For an example path client-A-B-C-pivot, then:
        //
        // A: downstream=client's connection id; upstream=B
        // B: downstream=A, upstream=C
        // C: downstream=B, upstream=pivot
        // pivot: downstream=C, upstream=pivot
        //
        // txid and rxid are joined in the same way, i.e. the txid of A equals the rxid of B,
        // and so on up the path.
        // TODO FIXME: this mixed terminology ("tx/rx" for hop ids, but "upstream/downstream"
        // for pubkeys) is needlessly confusing and should be unified, probably by renaming
        // txid and rxid to upstream_id and downstream_id.
        // TODO FIXME: why is router_id here at all?
        RouterID upstream;
        RouterID router_id;
        std::variant<RouterID, oxen::quic::ConnectionID> downstream;

        TransitHop() = default;

        // Shared secret between the client and this hop used for this hop's onion encryption
        SharedSecret shared_secret;

        // Used by each hop to mutate the encryption nonce used for the onion encryption of a
        // datum down a path.  This isn't cryptographically necessary (the same nonce could be
        // used all along) but rather is used to make traffic correlation more difficult.
        SymmNonce xor_nonce;

        std::chrono::milliseconds expiry{0s};
        std::chrono::milliseconds last_activity{0s};

        uint8_t version;
        bool terminal_hop{false};

        // Will be set to true immediately before being dropped from the path_context container;
        // this is primarily used by InboundRelaySession which also holds a shared_ptr (to avoid
        // needing to lock a weak ptr on every packet) to allow detection of when the TransitHop has
        // been dropped.
        bool is_dead{false};

        std::pair<std::variant<RouterID, oxen::quic::ConnectionID>, HopID> next_id(const HopID& h) const;

        // Returns true if this TransitHop matches the same transit components as other, that is,
        // has the same tx/rxids and upstream/downstream.  This is not equality, however, as this
        // returns true even if the shared secret, xor_nonce, expiry, etc. are different.
        bool same_transit(const TransitHop& other) const
        {
            return std::tie(txid, rxid, upstream, downstream)
                == std::tie(other.txid, other.rxid, other.upstream, other.downstream);
        }

        bool is_expired(std::chrono::milliseconds now = llarp::time_now_ms()) const { return now >= expiry; };

        nlohmann::json ExtractStatus() const;

        std::string to_string() const;
        static constexpr bool to_string_formattable = true;
    };

}  // namespace llarp::path
