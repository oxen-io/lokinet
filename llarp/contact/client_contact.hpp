#pragma once

#include "client_intro.hpp"
#include "tag.hpp"

#include <llarp/constants/version.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/dns/srv_data.hpp>
#include <llarp/net/policy.hpp>
#include <llarp/util/aligned.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/file.hpp>
#include <llarp/util/time.hpp>

#include <nlohmann/json_fwd.hpp>
#include <oxenc/bt_producer.h>

#include <unordered_set>
#include <vector>

namespace llarp
{
    struct EncryptedClientContact;

    namespace handlers
    {
        class SessionEndpoint;
    }

    // TESTNET:
    inline static constexpr auto CC_PUBLISH_INTERVAL{5min};

    /** ClientContact
        On the wire we encode the data as a dict containing:
            - "" : the CC format version, which must be == ClientContact::VERSION to be parsed successfully
            - "a" : public key of the remote client instance
            - "e" : (optional) exit policy containing sublists of accepted protocols and routed IP ranges
            - "i" : list of client introductions corresponding to the different pivots through which paths can be built
                    to the client instance
            - "p" : supported protocols indicating the traffic accepted by the client instance; this indicates if the
                    client is embedded and therefore requires a tunneled connection. Serialized as a bitwise flag of
                    protocol_flag enums (llarp/net/policy.hpp)
            - "s" : (optional) SRV records for lokinet DNS lookup
    */
    struct ClientContact
    {
        inline static constexpr uint8_t VERSION{0};

        ClientContact() = default;

        /// Constructs a ClientContact by parsing a serialized client contact value.  Throws if
        /// invalid.
        explicit ClientContact(std::span<const std::byte> buf);

        /** Parameters:
            - `private_data` : derived private subkey data
            - `pubkey` : master identity key pubkey
            - `srvs` : SRV records (optional, can be empty)
            - `proto_flags` : client-supported protocols
            - `policy` : exit-related traffic policy (optional)
         */
        ClientContact(
            PubKey pk,
            std::unordered_set<dns::SRVData> srvs,
            protocol_flag protocols,
            std::optional<net::ExitPolicy> policy = std::nullopt);

        // Encrypts and signs the client contact with the given blinded keypair
        EncryptedClientContact encrypt_and_sign(const Ed25519BlindedKey& blinded) const;

        /// Replaces the client intros in the current introset with the given values.  It is not
        /// necessary for the given values to be pre-sorted (i.e. this functions sorts them as
        /// required).
        void update_intros(std::vector<ClientIntro> intros);

        const PubKey& pubkey() const { return _pubkey; }
        // Returns the current intros; these will always be sorted in descending expiry order (i.e.
        // last entry is the first to expire).
        std::span<const ClientIntro> intros() const& { return _intros; }

        const std::unordered_set<dns::SRVData>& SRVs() const { return _srv; }

        protocol_flag protocols() const { return _protos; }

        const std::optional<net::ExitPolicy>& exit_policy() const { return _exit_policy; }

        bool is_expired(std::chrono::milliseconds now = llarp::time_now_ms()) const;

      private:
        PubKey _pubkey;

        std::vector<ClientIntro> _intros;
        std::unordered_set<dns::SRVData> _srv;

        protocol_flag _protos;

        // In exit mode, we advertise our policy for accepted traffic and the corresponding ranges
        std::optional<net::ExitPolicy> _exit_policy;

        std::vector<std::byte> bt_encode() const;

        session_tag generate_session_tag() const;

      public:
        bool operator==(const ClientContact& other) const
        {
            return std::tie(_pubkey, _intros, _srv, _protos, _exit_policy)
                == std::tie(other._pubkey, other._intros, other._srv, other._protos, other._exit_policy);
        }

        std::string to_string() const;
        static constexpr bool to_string_formattable = true;
    };

    // TODO: there should be a limit on how large an encCC we will store on relays, and we should
    // check that when we generate & sign as well to make sure we don't exceed it.
    //
    /** EncryptedClientContact
            "i" blinded local Ed25519 pubkey
            "n" nonce
            "t" signing time
            "x" encrypted payload
            "~" signature   (signed with blinded derived scalar `b`)
    */
    struct EncryptedClientContact
    {
        EncryptedClientContact() : nonce{SymmNonce::make_random()} {}

        explicit EncryptedClientContact(std::span<const std::byte> buf);
        explicit EncryptedClientContact(std::string buf);

      private:
        friend struct ClientContact;

        PubKey blinded_pubkey;
        SymmNonce nonce;
        std::chrono::milliseconds signed_at{0s};
        std::vector<std::byte> encrypted;

        std::string _bt_payload;

        // Returns a dict-in-progress containing everything except for the ~ signature.
        [[nodiscard]] oxenc::bt_dict_producer bt_encode_for_signing() const;

        void bt_decode(oxenc::bt_dict_consumer&& btdc);

      public:
        const PubKey& key() const { return blinded_pubkey; }

        std::optional<ClientContact> decrypt(const PubKey& root) const;

        std::string_view bt_payload() const { return _bt_payload; }

        bool is_expired(std::chrono::milliseconds now = time_now_ms()) const;

        bool newer_than(const EncryptedClientContact& that) const { return signed_at > that.signed_at; }
    };
}  //  namespace llarp
