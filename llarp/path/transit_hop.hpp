#pragma once

#include "path_types.hpp"

#include <llarp/constants/path.hpp>
#include <llarp/contact/router_id.hpp>
#include <llarp/util/compare_ptr.hpp>

namespace llarp
{
    class Router;

    namespace path
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
        // expiry, and so on.  It is primarily just a container to hold this data.
        struct TransitHop
        {
            HopID txid, rxid;

            // Along a path "upstream" is the next router away from the client, "downstream" is the
            // hop towards the client.  The pivot (which has no upstream) is identified by the
            // upstream value being equal to itself.
            //
            // For an example path client-A-B-C-pivot, then:
            //
            // A: downstream=client's ephemeral key; upstream=B
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
            RouterID downstream;

            TransitHop() = default;

            // Shared secret between the client and this hop used for this hop's onion encryption
            SharedSecret shared_secret;

            // Used by each hop to mutate the encryption nonce used for the onion encryption of a
            // datum down a path.  This isn't cryptographically necessary (the same nonce could be
            // used all along) but rather is used to make traffic correlation more difficult.
            SymmNonce xor_nonce;

            std::chrono::milliseconds expiry{0s};

            uint8_t version;
            std::chrono::milliseconds last_activity{0s};
            bool terminal_hop{false};

            std::optional<std::pair<RouterID, HopID>> next_id(const HopID& h) const;

            bool operator==(const TransitHop& other) const
            {
                return std::tie(txid, rxid, upstream, downstream)
                    == std::tie(other.txid, other.rxid, other.upstream, other.downstream);
            }

            bool is_expired(std::chrono::milliseconds now = llarp::time_now_ms()) const { return now >= expiry; };

            nlohmann::json ExtractStatus() const;

            std::string to_string() const;
            static constexpr bool to_string_formattable = true;
        };

        // InboundRelayPath is a path-like object, used only by InboundRelaySession, containing the
        // locally visibility end of a path through the router (i.e. it just sees one hop in each
        // direction along the path, but unlike a client path, does know anything beyond that).
        //
        // TODO FIXME: this class seems unnecessary: this is only used for an InboundRelaySession,
        // and it seems like that class could just absorb this to make life easier everywhere.
        struct InboundRelayPath final : public TransitHop, public session_path_interface
        {
          private:
            handlers::SessionEndpoint& _parent;

            void encrypt_path_message(
                std::vector<std::byte>& payload,
                SymmNonce&& nonce = SymmNonce::make_random(),
                std::byte type = std::byte{0x01});

          public:
            InboundRelayPath(const TransitHop& hop, handlers::SessionEndpoint& p);

            void send_path_control_message(
                std::string_view method,
                std::span<const std::byte> body,
                std::function<void(quic::message)> func,
                std::byte type = std::byte{0x01}) override;

            void send_path_data_message(
                std::vector<std::byte>&& body,
                SymmNonce&& nonce = SymmNonce::make_random(),
                std::byte type = std::byte{0x01}) override;

            RouterID terminal_rid() const override { return router_id; }
            HopID terminal_hopid() const override { return txid; }

            std::string to_string() const override;
        };
    }  // namespace path
}  // namespace llarp
