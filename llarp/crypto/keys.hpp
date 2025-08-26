#pragma once

#include <llarp/crypto/constants.hpp>
#include <llarp/util/aligned.hpp>
#include <llarp/util/buffer.hpp>

namespace llarp
{
    struct PubKey : public AlignedBuffer<PUBKEYSIZE>
    {
        using AlignedBuffer<PUBKEYSIZE>::AlignedBuffer;

        bool from_hex(const std::string& str);

        std::string to_string() const;

        // FIXME TODO revisit this
        PubKey& operator=(const uint8_t* ptr);
    };

    struct PubKey;
    struct Ed25519BlindedKey;

    /// Stores a sodium "secret key" value, which is actually the Ed25519 seed
    /// concatenated with the public key.  Note that the seed is *not* the private
    /// key value itself, but rather the seed from which it can be calculated.
    struct Ed25519SecretKey final : AlignedBuffer<SECKEYSIZE>
    {
        using AlignedBuffer<SECKEYSIZE>::AlignedBuffer;

        // If constructed with just the seed, we recalculate the pubkey
        explicit Ed25519SecretKey(const AlignedBuffer<32>& seed)
        {
            std::memcpy(data(), seed.data(), seed.size());
            recalculate();
        }

        /// recalculate public component (last 32 bytes) from leading 32-byte seed component
        void recalculate();

        /// Verifies that the public component matches the seed component
        bool check_pubkey() const;

        std::span<const std::byte, 32> pubkey_span() const { return span().last<32>(); }
        PubKey to_pubkey() const { return PubKey{pubkey_span()}; }

        void sign(std::span<std::byte, SIGSIZE> out, std::span<const std::byte> buf) const;
        std::array<std::byte, SIGSIZE> sign(std::span<const std::byte> buf) const;

        static constexpr bool to_string_formattable{false};
    };

    /// Stores a private scalar, pubkey, and hash_data for a blinded key (typically) derived from a
    /// Ed25519SecretKey.
    struct Ed25519BlindedKey final
    {
        AlignedBuffer<32> scalar;
        AlignedBuffer<32> hash_data;
        PubKey pubkey;

        // Constructs as blinded key as the result of blinding an Ed25519SecretKey
        Ed25519BlindedKey(const Ed25519SecretKey& root, std::string_view blind_domain);

        // Constructs a blinded key from a scalar and hash data.  The pubkey is derived during
        // construction from the scalar.
        Ed25519BlindedKey(std::span<const std::byte, 32> scalar, std::span<const std::byte, 32> hash_data);

        // Produces an Ed25519 signature that validates with this blinded key's pubkey
        void sign(std::span<std::byte, SIGSIZE> out, std::span<const std::byte> buf) const;
        std::array<std::byte, SIGSIZE> sign(std::span<const std::byte> buf) const;

        static constexpr bool to_string_formattable{false};
    };

}  // namespace llarp

namespace std
{
    template <>
    struct hash<llarp::PubKey> : public hash<llarp::AlignedBuffer<PUBKEYSIZE>>
    {};
}  //  namespace std
