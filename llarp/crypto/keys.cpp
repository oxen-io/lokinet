#include "keys.hpp"

#include "crypto.hpp"

#include <llarp/util/bspan.hpp>
#include <llarp/util/logging.hpp>

#include <sodium/crypto_core_ed25519.h>
#include <sodium/crypto_generichash_blake2b.h>
#include <sodium/crypto_scalarmult_ed25519.h>
#include <sodium/crypto_sign.h>
#include <sodium/utils.h>

namespace llarp
{

    static auto logcat = log::Cat("keys");

    bool PubKey::from_hex(const std::string& str)
    {
        if (str.size() != 2 * size())
            return false;
        oxenc::from_hex(str.begin(), str.end(), begin());
        return true;
    }

    std::string PubKey::to_string() const { return oxenc::to_base32z(begin(), end()); }

    PubKey& PubKey::operator=(const uint8_t* ptr)
    {
        std::copy(ptr, ptr + SIZE, begin());
        return *this;
    }

    bool Ed25519SecretKey::check_pubkey() const
    {
        std::array<unsigned char, 32> pk;
        std::array<unsigned char, 64> sk;
        crypto_sign_seed_keypair(pk.data(), sk.data(), data());
        return 0 == std::memcmp(pk.data(), pubkey_span().data(), 32);
    }

    void Ed25519SecretKey::recalculate()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        std::array<unsigned char, 32> pk;
        std::array<unsigned char, 64> sk;
        crypto_sign_seed_keypair(pk.data(), sk.data(), data());
        std::memcpy(data() + 32, pk.data(), 32);
    }

    void Ed25519SecretKey::sign(std::span<std::byte, SIGSIZE> sig, std::span<const std::byte> buf) const
    {
        crypto_sign_detached(as_uspan(sig).data(), nullptr, as_uspan(buf).data(), buf.size(), data());
    }

    std::array<std::byte, SIGSIZE> Ed25519SecretKey::sign(std::span<const std::byte> buf) const
    {
        std::array<std::byte, SIGSIZE> sig;
        sign(sig, buf);
        return sig;
    }

    void Ed25519BlindedKey::sign(std::span<std::byte, SIGSIZE> sig, std::span<const std::byte> buf) const
    {
        auto ubuf = as_uspan(buf);

        // r = H(s || M) where here s is pseudorandom bytes, generated under standard Ed25519 as
        // part of hashing the seed (i.e. [a,s] = H(k), ignoring `a` clamping).  For blinded keys,
        // however, we instead use `s` created at construction of the BlindedKey object (which is a
        // domain-keyed blake2b hash of the root seed, rather than the second half of a SHA512 of
        // the root key).
        unsigned char nonce[64];
        crypto_hash_sha512_state hs;
        crypto_hash_sha512_init(&hs);
        crypto_hash_sha512_update(&hs, hash_data.data(), hash_data.size());
        crypto_hash_sha512_update(&hs, ubuf.data(), ubuf.size());
        crypto_hash_sha512_final(&hs, nonce);
        crypto_core_ed25519_scalar_reduce(nonce, nonce);

        // Final signature consists of R || S:
        auto R = as_uspan(sig.first<32>());
        auto S = as_uspan(sig.last<32>());

        // R = r * G, store directly into sig to make: sig = (R || uninitialized)
        crypto_scalarmult_ed25519_base_noclamp(R.data(), nonce);

        // hram = H(R || A || M)
        unsigned char hram[64];
        crypto_hash_sha512_init(&hs);
        crypto_hash_sha512_update(&hs, R.data(), 32);
        crypto_hash_sha512_update(&hs, pubkey.data(), 32);
        crypto_hash_sha512_update(&hs, ubuf.data(), ubuf.size());
        crypto_hash_sha512_final(&hs, hram);

        // S = r + H(R || A || M) * a, and we store S directly into sig:
        crypto_core_ed25519_scalar_reduce(hram, hram);
        unsigned char mulres[32];
        crypto_core_ed25519_scalar_mul(mulres, hram, scalar.data());
        crypto_core_ed25519_scalar_add(S.data(), mulres, nonce);

        sodium_memzero(nonce, sizeof nonce);
    }

    std::array<std::byte, SIGSIZE> Ed25519BlindedKey::sign(std::span<const std::byte> buf) const
    {
        std::array<std::byte, SIGSIZE> sig;
        sign(sig, buf);
        return sig;
    }

    Ed25519BlindedKey::Ed25519BlindedKey(std::span<const std::byte, 32> sc, std::span<const std::byte, 32> hd)
        : scalar{sc}, hash_data{hd}
    {
        crypto_scalarmult_ed25519_base_noclamp(pubkey.data(), scalar.data());
    }

    Ed25519BlindedKey::Ed25519BlindedKey(const Ed25519SecretKey& root, std::string_view blind_domain)
    {
        // This function's name is a bit misleading: what it actually does is convert the seed to the
        // Ed25519 private scalar, because that's the operation you do when converting Ed -> X (i.e.
        // an X secret key *is* a private scalar, unlike Ed keys).  We don't care at all about X,
        // but we do want the private scalar and this gives us exactly that.
        //
        // a = clamp(SHA512(seed)[0:32])
        crypto_sign_ed25519_sk_to_curve25519(scalar.data(), root.data());

        // f = blinding factor
        auto bfactor = crypto::blinding_scalar(root.pubkey_span(), blind_domain);

        // b = af
        crypto_core_ed25519_scalar_mul(scalar.data(), scalar.data(), bfactor.data());

        // Now compute the pubkey from the scalar b:
        // B = bG
        if (0 != crypto_scalarmult_ed25519_base_noclamp(pubkey.data(), scalar.data()))
            throw std::runtime_error{"Keypair blinding failed!"};

        // In regular Ed25519, the hash data (used during signing) is derived from the hash of seed
        // (i.e. the second half of the SHA512 operation above).  It seems preferable, however, to
        // not use identical hash data for a subkey, so instead we do our own keyed hash of the seed
        // to get some equivalent (but different valued) hash data for the same purpose.
        crypto_generichash_blake2b(
            hash_data.data(),
            hash_data.size(),
            reinterpret_cast<const unsigned char*>(root.data()),
            32,
            reinterpret_cast<const unsigned char*>(blind_domain.data()),
            blind_domain.size());
    }

}  // namespace llarp
