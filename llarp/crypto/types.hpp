#pragma once

#include "constants.hpp"

#include <llarp/constants/files.hpp>
#include <llarp/contact/keys.hpp>
#include <llarp/util/aligned.hpp>
#include <llarp/util/buffer.hpp>

#include <algorithm>

namespace llarp
{
    using SharedSecret = AlignedBuffer<SHAREDKEYSIZE>;

    struct RemoteRC;
    struct RouterID;
    struct PubKey;
    struct Ed25519PrivateData;

    /// Stores a sodium "secret key" value, which is actually the Ed25519 seed
    /// concatenated with the public key.  Note that the seed is *not* the private
    /// key value itself, but rather the seed from which it can be calculated.
    struct Ed25519SecretKey final : AlignedBuffer<SECKEYSIZE>
    {
        using AlignedBuffer<SECKEYSIZE>::AlignedBuffer;

        // If constructed with just the seed, we recalculate the pubkey
        explicit Ed25519SecretKey(const AlignedBuffer<32>& seed)
        {
            std::copy(seed.begin(), seed.end(), begin());
            recalculate();
        }

        /// recalculate public component
        bool recalculate();

        PubKey to_pubkey() const;
        std::span<const std::byte, 32> pubkey_span() { return span().last<32>(); }

        Ed25519PrivateData to_eddata() const;

        Ed25519PrivateData derive_private_subkey_data(uint64_t domain = 1) const;

        bool load_from_file(const fs::path& fname);

        bool write_to_file(const fs::path& fname) const;

        std::string_view to_string() const { return "[secretkey]"; }
        static constexpr bool to_string_formattable{true};
    };

    /// Ed25519PrivateData is similar to Ed25519SecretKey except that it only stores the
    /// private scalar and a hash, unlike SecretKey which stores the seed from which
    /// the private key and hash value are generated.
    struct Ed25519PrivateData final : public AlignedBuffer<64>
    {
        using AlignedBuffer<64>::AlignedBuffer;

        // Returns writeable access to the 32-byte Ed25519 Private Scalar
        std::span<uint8_t, 32> scalar() { return std::span<uint8_t, 32>{data(), 32}; }
        // Returns readable access to the 32-byte Ed25519 Private Scalar
        std::span<const uint8_t, 32> scalar() const { return std::span<const uint8_t, 32>{data(), 32}; }
        // Returns writeable access to the 32-byte Ed25519 Signing Hash
        std::span<uint8_t, 32> signing_hash() { return std::span<uint8_t, 32>{data() + 32, 32}; }
        // Returns readable access to the 32-byte Ed25519 Signing Hash
        std::span<const uint8_t, 32> signing_hash() const { return std::span<const uint8_t, 32>{data() + 32, 32}; }

        PubKey to_pubkey() const;

        std::string_view to_string() const { return "[privatekey]"; }
        static constexpr bool to_string_formattable{true};
    };

    struct Signature final : public AlignedBuffer<SIGSIZE>
    {};

    struct SymmNonce final : public AlignedBuffer<NONCESIZE>
    {
        using AlignedBuffer<NONCESIZE>::AlignedBuffer;

        SymmNonce operator^(const SymmNonce& other) const
        {
            SymmNonce ret;
            std::transform(begin(), end(), other.begin(), ret.begin(), std::bit_xor<>());
            return ret;
        }

        static SymmNonce make_random();
    };

    /// Holds all the data used for symmetric DH key-exchange (ex: path-build, session-init, etc)
    struct shared_kx_data
    {
        shared_kx_data() = default;

      private:
        shared_kx_data(Ed25519SecretKey&& sk);

      public:
        Ed25519SecretKey ephemeral_key;
        PubKey pubkey{};
        SharedSecret shared_secret{};
        SymmNonce nonce{SymmNonce::make_random()};
        SymmNonce xor_nonce{SymmNonce::make_random()};

        void generate_xor();

        static shared_kx_data generate();

        void client_dh(const RouterID& remote);

        void server_dh(const Ed25519SecretKey& local_sk);

        void encrypt(std::span<std::byte> data);

        void decrypt(std::span<std::byte> enc);
    };

    struct hash_key : public AlignedBuffer<32>
    {
        using AlignedBuffer<32>::AlignedBuffer;

        std::string to_string() const;

        static hash_key derive_from_rid(PubKey root);

        hash_key operator^(const hash_key& other) const
        {
            hash_key dist;
            std::transform(begin(), end(), other.begin(), dist.begin(), std::bit_xor<>());
            return dist;
        }

        bool operator==(const hash_key& other) const { return as_array() == other.as_array(); }
    };

    // FIXME: this metric is wrong, because of vanity pubkeys.  Switch to:
    // - xor all values of both keys as u64's
    // - use (xored value, pubkey) for sorting (so if we collide, we tie break with pubkey)
    struct XorMetric
    {
        const hash_key us;

        XorMetric(hash_key ourKey) : us{std::move(ourKey)} {}

        bool operator()(const hash_key& left, const hash_key& right) const;
        bool operator()(const RemoteRC& left, const RemoteRC& right) const;
    };

}  // namespace llarp
