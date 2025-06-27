#pragma once

#include "types.hpp"

#include <llarp/contact/router_id.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/random.hpp>

#include <cstdint>

namespace llarp::crypto
{
    /// decrypt cipherText given the key generated from name
    std::optional<RouterID> maybe_decrypt_name(std::string_view ciphertext, SymmNonce nonce, std::string_view name);

    /// xchacha symmetric cipher
    bool xchacha20(std::span<std::byte> buf, const SharedSecret&, const SymmNonce&);

    SymmNonce onion(
        std::span<std::byte> buf, const SharedSecret& k, const SymmNonce& nonce, const SymmNonce& xor_factor);

    /// path dh creator's side
    bool dh_client(SharedSecret&, const PubKey&, const Ed25519SecretKey&, const SymmNonce&);

    /// path dh relay side
    bool dh_server(SharedSecret&, const PubKey&, const Ed25519SecretKey&, const SymmNonce&);
    bool dh_server(uint8_t* shared_secret, const uint8_t* other_pk, const uint8_t* local_sk, const uint8_t* nonce);

    /// blake2b 256 bit
    void shorthash(std::span<std::byte, SHORTHASHSIZE> out, std::span<const std::byte> buf);
    AlignedBuffer<SHORTHASHSIZE> shorthash(std::span<const std::byte> buf);

    /// ed25519 sign
    bool sign(std::span<std::byte, SIGSIZE> out, const Ed25519SecretKey& secret, std::span<const std::byte> buf);

    /// ed25519 sign (custom with derived keys)
    bool sign(std::span<std::byte, SIGSIZE> out, const Ed25519PrivateData& privkey, std::span<const std::byte> buf);

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

    std::array<unsigned char, 32> make_scalar(const PubKey& k, uint64_t domain);

    /// derive sub keys for public keys.  hash is really only intended for
    /// testing ands key_n if given.
    bool derive_subkey(uint8_t* derived, size_t derived_len, const PubKey& root, uint64_t key_n);

    Ed25519SecretKey generate_identity();

    bool check_identity_privkey(const Ed25519SecretKey&);

    bool check_passwd_hash(std::string pwhash, std::string challenge);

}  // namespace llarp::crypto
