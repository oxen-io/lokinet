#pragma once

#include "keys.hpp"
#include "types.hpp"

#include <llarp/contact/router_id.hpp>

#include <cstdint>

namespace llarp
{
    using namespace std::literals;
}

namespace llarp::crypto
{
    /// decrypt cipherText given the key generated from name
    std::optional<RouterID> maybe_decrypt_name(std::string_view ciphertext, SymmNonce nonce, std::string_view name);

    /// xchacha symmetric cipher
    void xchacha20(std::span<std::byte> buf, const SharedSecret&, const SymmNonce&);

    /// path dh creator's side
    ///
    /// Note that the input "nonce" here is used domain separation in the shared secret generation,
    /// but isn't used as an encryption nonce (i.e. the same nonce can be safely used for both
    /// shared secret generation and an initial payload encryption).
    bool dh_client(
        SharedSecret& out, const PubKey& server_pk, const Ed25519SecretKey& client_seckey, const SymmNonce& nonce);

    /// Generates an ephemeral keypair and random nonce, calls dh_client, then returns the resulting
    /// shared secret, the ephemeral pubkey, and the nonce.  Throws std::invalid_argument if the
    /// server pk is not valid.
    std::tuple<SharedSecret, PubKey, SymmNonce> dh_client_gen(const PubKey& server_pk);

    /// path dh relay side
    bool dh_server(
        SharedSecret& out, const PubKey& client_pk, const Ed25519SecretKey& server_seckey, const SymmNonce& nonce);
    bool dh_server(uint8_t* shared_secret, const uint8_t* other_pk, const uint8_t* local_sk, const uint8_t* nonce);

    /// blake2b 256 bit
    void shorthash(std::span<std::byte, SHORTHASHSIZE> out, std::span<const std::byte> buf);
    AlignedBuffer<SHORTHASHSIZE> shorthash(std::span<const std::byte> buf);

    /// ed25519 verify
    bool verify(
        std::span<const std::byte, PUBKEYSIZE> pub,
        std::span<const std::byte> data,
        std::span<const std::byte, SIGSIZE> sig);

    /// Used in path-build and session initiation messages. Derives a shared secret key for symmetric DH, encrypting
    /// the given payload in-place. Will throw on failure of either the client DH derivation or the xchacha20
    /// payload mutation
    void derive_encrypt_outer_wrapping(
        const Ed25519SecretKey& shared_key,
        SharedSecret& secret,
        const SymmNonce& nonce,
        const RouterID& remote,
        std::span<std::byte> payload);

    /// Used in receiving path-build and session initiation messages. Derives a shared secret key using an ephemeral
    /// pubkey and the provided nonce. The encrypted payload is mutated in-place. Will throw on failure of either
    /// the server DH derivation or the xchacha20 payload mutation
    void derive_decrypt_outer_wrapping(
        const Ed25519SecretKey& local,
        SharedSecret& shared,
        const PubKey& remote,
        const SymmNonce& nonce,
        std::span<std::byte> encrypted);

    /// Returns the Ed25519 scalar used for blinding of the given pubkey with the given
    /// blind_domain.  See `blind`.
    ///
    /// This scalar can be used with either the root private scalar or
    /// the root pubkey to produce the blinded private scalar or blinded pubkey, respectively.
    ///
    /// `pubkey` is usually simply passed via implicit conversion from a `PubKey` argument.
    std::array<unsigned char, 32> blinding_scalar(std::span<const std::byte, 32> pubkey, std::string_view blind_domain);

    /// Derive a blinded pubkey from a root pubkey and blinding domain and stores it in `derived`.
    ///
    /// blind_domain should be between 0 and 64 characters long (longer values will be truncated),
    /// and generally should be one of the constants defined below.  A different blind_domain should
    /// be used for each distinct blinding type, and will produce unrelated blinded keys.
    ///
    /// Returns true if successful, false if `root` is not a valid pubkey.
    bool blind(PubKey& blinded, const PubKey& root, std::string_view blind_domain);

    // Known values for the `blind_domain` argument of blind(), blinding_scalar(), and the
    // Ed25519BlindedKey constructor.
    namespace blinding
    {
        constexpr auto CLIENT_CONTACT = "SessionRouterClientContact"sv;
    }

    Ed25519SecretKey generate_ed25519();

    // Verifies that the cached pubkey embedded in `keys` correctly corresponds with the seed value
    // in `keys`; effectively this checks for corruption of the keys value, such as when loading the
    // keypair from disk.
    bool check_pubkey(const Ed25519SecretKey& keys);

    bool check_passwd_hash(std::string pwhash, std::string challenge);

}  // namespace llarp::crypto
