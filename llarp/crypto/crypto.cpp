#include "crypto.hpp"

#include <llarp/contact/keys.hpp>
#include <llarp/util/bspan.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/util/random.hpp>

#include <oxenc/endian.h>
#include <sodium/core.h>
#include <sodium/crypto_aead_xchacha20poly1305.h>
#include <sodium/crypto_core_ed25519.h>
#include <sodium/crypto_generichash.h>
#include <sodium/crypto_scalarmult_curve25519.h>
#include <sodium/crypto_scalarmult_ed25519.h>
#include <sodium/crypto_sign.h>
#include <sodium/crypto_stream_xchacha20.h>
#include <sodium/utils.h>

#include <cassert>
#include <cstring>
#ifdef HAVE_CRYPT
#include <crypt.h>
#endif

namespace llarp::crypto
{
    static auto logcat = log::Cat("crypto");

    static bool dh(
        SharedSecret& out,
        const PubKey& client_pk,
        const PubKey& server_pk,
        const uint8_t* themPub,
        const Ed25519PrivateData& local_edhash)
    {
        SharedSecret shared;
        crypto_generichash_state h;

        if (crypto_scalarmult_ed25519(shared.data(), local_edhash.scalar().data(), themPub))
        {
            return false;
        }

        log::trace(
            logcat,
            "client-pk: {}, server-pk: {}, shared secret: {}",
            client_pk.to_string(),
            server_pk.to_string(),
            shared.to_string());

        crypto_generichash_blake2b_init(&h, nullptr, 0U, shared.size());
        crypto_generichash_blake2b_update(&h, client_pk.data(), client_pk.size());
        crypto_generichash_blake2b_update(&h, server_pk.data(), server_pk.size());
        crypto_generichash_blake2b_update(&h, shared.data(), shared.size());
        crypto_generichash_blake2b_final(&h, out.data(), out.size());
        return true;
    }

    static bool dh_client_priv(SharedSecret& shared, const PubKey& pk, const Ed25519SecretKey& sk, const SymmNonce& n)
    {
        SharedSecret dh_result;

        if (dh(dh_result, sk.to_pubkey(), pk, pk.data(), sk.to_eddata()))
        {
            return crypto_generichash_blake2b(
                       shared.data(), shared.size(), n.data(), n.size(), dh_result.data(), dh_result.size())
                != -1;
        }

        log::warning(logcat, "dh_client - dh failed");
        return false;
    }

    static bool dh_server_priv(SharedSecret& shared, const PubKey& pk, const Ed25519SecretKey& sk, const SymmNonce& n)
    {
        SharedSecret dh_result;

        if (dh(dh_result, pk, sk.to_pubkey(), pk.data(), sk.to_eddata()))
        {
            return crypto_generichash_blake2b(
                       shared.data(), shared.size(), n.data(), n.size(), dh_result.data(), dh_result.size())
                != -1;
        }

        log::warning(logcat, "dh_server - dh failed");
        return false;
    }

    std::optional<RouterID> maybe_decrypt_name(std::string_view ciphertext, SymmNonce nonce, std::string_view namestr)
    {
        const auto payloadsize = ciphertext.size() - crypto_aead_xchacha20poly1305_ietf_ABYTES;
        if (payloadsize != 32)
            return std::nullopt;

        auto bname = as_bspan(namestr);

        // The (unkeyed) blake2b hash of the name is the (public) SNS storage key:
        auto namehash = shorthash(bname);

        // The value itself is encrypted with a symmetric key that is also a blake2b hash of the
        // name, but using a hash key to make it unrelated to the public hash:
        AlignedBuffer<32> derivedKey;
        crypto_generichash_blake2b(
            derivedKey.data(),
            derivedKey.size(),
            as_uspan(bname).data(),
            bname.size(),
            namehash.data(),
            namehash.size());

        auto result = std::make_optional<RouterID>();
        if (crypto_aead_xchacha20poly1305_ietf_decrypt(
                result->data(),
                nullptr,
                nullptr,
                reinterpret_cast<const uint8_t*>(ciphertext.data()),
                ciphertext.size(),
                nullptr,
                0,
                nonce.data(),
                derivedKey.data())
            != 0)
            return std::nullopt;

        return result;
    }

    bool xchacha20(std::span<std::byte> buf, const SharedSecret& secret, const SymmNonce& nonce)
    {
        auto ubuf = as_uspan(buf);
        return crypto_stream_xchacha20_xor(ubuf.data(), ubuf.data(), ubuf.size(), nonce.data(), secret.data()) == 0;
    }

    // do a round of chacha for and return the nonce xor the given xor_factor
    SymmNonce onion(
        std::span<std::byte> buf, const SharedSecret& k, const SymmNonce& nonce, const SymmNonce& xor_factor)
    {
        if (!xchacha20(buf, k, nonce))
            throw std::runtime_error{"chacha failed during onion step"};

        return nonce ^ xor_factor;
    }

    bool dh_client(SharedSecret& shared, const PubKey& pk, const Ed25519SecretKey& sk, const SymmNonce& n)
    {
        return dh_client_priv(shared, pk, sk, n);
    }

    /// path dh relay side
    bool dh_server(SharedSecret& shared, const PubKey& pk, const Ed25519SecretKey& sk, const SymmNonce& n)
    {
        return dh_server_priv(shared, pk, sk, n);
    }

    void shorthash(std::span<std::byte, SHORTHASHSIZE> result, std::span<const std::byte> buf)
    {
        crypto_generichash_blake2b(
            reinterpret_cast<unsigned char*>(result.data()),
            result.size(),
            reinterpret_cast<const unsigned char*>(buf.data()),
            buf.size(),
            nullptr,
            0);
    }
    AlignedBuffer<SHORTHASHSIZE> shorthash(std::span<const std::byte> buf)
    {
        AlignedBuffer<SHORTHASHSIZE> result;
        shorthash(result, buf);
        return result;
    }
    bool sign(std::span<std::byte, SIGSIZE> sig, const Ed25519SecretKey& secret, std::span<const std::byte> buf)
    {
        return crypto_sign_detached(as_uspan(sig).data(), nullptr, as_uspan(buf).data(), buf.size(), secret.data())
            != -1;
    }

    bool sign(std::span<std::byte, SIGSIZE> sig, const Ed25519PrivateData& privkey, std::span<const std::byte> buf)
    {
        PubKey pubkey = privkey.to_pubkey();

        crypto_hash_sha512_state hs;
        unsigned char nonce[64];
        unsigned char hram[64];
        unsigned char mulres[32];

        // r = H(s || M) where here s is pseudorandom bytes typically generated as
        // part of hashing the seed (i.e. [a,s] = H(k)), but for derived
        // PrivateKeys will come from a hash of the root key's s concatenated with
        // the derivation hash.
        crypto_hash_sha512_init(&hs);
        crypto_hash_sha512_update(&hs, privkey.signing_hash().data(), 32);
        crypto_hash_sha512_update(&hs, as_uspan(buf).data(), buf.size());
        crypto_hash_sha512_final(&hs, nonce);
        crypto_core_ed25519_scalar_reduce(nonce, nonce);

        // copy pubkey into sig to make (for now) sig = (R || A)
        memmove(sig.data() + 32, pubkey.data(), 32);

        auto* sig_data = as_uspan(sig).data();
        // R = r * B
        crypto_scalarmult_ed25519_base_noclamp(sig_data, nonce);

        // hram = H(R || A || M)
        crypto_hash_sha512_init(&hs);
        crypto_hash_sha512_update(&hs, sig_data, 64);
        crypto_hash_sha512_update(&hs, as_uspan(buf).data(), buf.size());
        crypto_hash_sha512_final(&hs, hram);

        // S = r + H(R || A || M) * s, so sig = (R || S)
        crypto_core_ed25519_scalar_reduce(hram, hram);
        crypto_core_ed25519_scalar_mul(mulres, hram, privkey.data());
        crypto_core_ed25519_scalar_add(sig_data + 32, mulres, nonce);

        sodium_memzero(nonce, sizeof nonce);

        return true;
    }

    bool verify(
        std::span<const std::byte, PUBKEYSIZE> pub,
        std::span<const std::byte> data,
        std::span<const std::byte, SIGSIZE> sig)
    {
        return crypto_sign_verify_detached(
                   as_uspan(sig).data(), as_uspan(data).data(), data.size(), as_uspan(pub).data())
            != -1;
    }

    void derive_encrypt_outer_wrapping(
        const Ed25519SecretKey& shared_key,
        SharedSecret& secret,
        const SymmNonce& nonce,
        const RouterID& remote,
        std::span<std::byte> payload)
    {
        // derive shared key
        if (!dh_client(secret, remote, shared_key, nonce))
        {
            auto err = "DH client failed during shared key derivation!"s;
            log::warning(logcat, "{}", err);
            throw std::runtime_error{"err"};
        }

        // encrypt hop_info (mutates in-place)
        if (!xchacha20(payload, secret, nonce))
        {
            auto err = "Payload symmetric encryption failed!"s;
            log::warning(logcat, "{}", err);
            throw std::runtime_error{err};
        }
    }

    void derive_decrypt_outer_wrapping(
        const Ed25519SecretKey& local_sk,
        SharedSecret& shared,
        const PubKey& remote,
        const SymmNonce& nonce,
        std::span<std::byte> encrypted)
    {
        // derive shared secret using shared secret and our secret key (and nonce)
        if (!dh_server(shared, remote, local_sk, nonce))
        {
            auto err = "DH server failed during shared key derivation!"s;
            log::warning(logcat, "{}", err);
            throw std::runtime_error{err};
        }

        // decrypt hop_info (mutates in-place)
        if (!xchacha20(encrypted, shared, nonce))
        {
            auto err = "Payload symmetric decryption failed!"s;
            log::warning(logcat, "{}", err);
            throw std::runtime_error{err};
        }

        log::trace(logcat, "Shared secret: {}", shared.to_string());
    }

    /// clamp a 32 byte ec point
    static void clamp_ed25519(uint8_t* out)
    {
        out[0] &= 248;
        out[31] &= 127;
        out[31] |= 64;
    }

    template <typename K>
    static K clamp(const K& p)
    {
        K out = p;
        clamp_ed25519(out);
        return out;
    }

    template <typename K>
    static bool is_clamped(const K& key)
    {
        K other(key);
        clamp_ed25519(other.data());
        return other == key;
    }

    static constexpr char derived_key_hash_str[161] =
        "just imagine what would happen if we all decided to understand. you "
        "can't in the and by be or then before so just face it this text hurts "
        "to read? lokinet yolo!";

    std::array<unsigned char, 32> make_scalar(const PubKey& k, uint64_t domain)
    {
        // b = BLIND-STRING || k || i
        std::array<uint8_t, 160 + PubKey::SIZE + sizeof(uint64_t)> buf;
        std::copy(derived_key_hash_str, derived_key_hash_str + 160, buf.begin());
        std::copy(k.begin(), k.end(), buf.begin() + 160);
        oxenc::write_host_as_little(domain, buf.data() + 160 + PubKey::SIZE);

        // n = H(b)
        // h = make_point(n)
        std::array<unsigned char, 64> n;
        std::array<unsigned char, 32> out;

        crypto_generichash_blake2b(n.data(), n.size(), buf.data(), buf.size(), nullptr, 0);
        crypto_core_ed25519_scalar_reduce(out.data(), n.data());

        return out;
    }

    bool derive_subkey(uint8_t* derived, size_t derived_len, const PubKey& root_pubkey, uint64_t key_n)
    {
        if (derived_len != PubKey::SIZE)
        {
            log::error(logcat, "Derived pubkey must be {}!", PubKey::SIZE);
            return false;
        }

        // scalar h = H( BLIND-STRING || root_pubkey || key_n )
        std::array<unsigned char, 32> h = make_scalar(root_pubkey, key_n);
        return 0 == crypto_scalarmult_ed25519_noclamp(derived, h.data(), root_pubkey.data());
    }

    Ed25519SecretKey generate_identity()
    {
        Ed25519SecretKey ret{};
        PubKey pk;
        [[maybe_unused]] int result = crypto_sign_ed25519_keypair(pk.data(), ret.data());
        assert(result != -1);
        const PubKey sk_pk = ret.to_pubkey();
        (void)sk_pk;
        assert(pk == sk_pk);
        return ret;
    }

    bool check_identity_privkey(const Ed25519SecretKey& keys)
    {
        AlignedBuffer<crypto_sign_SEEDBYTES> seed;
        PubKey pk;
        Ed25519SecretKey sk;
        if (crypto_sign_ed25519_sk_to_seed(seed.data(), keys.data()) == -1)
            return false;
        if (crypto_sign_seed_keypair(pk.data(), sk.data(), seed.data()) == -1)
            return false;
        return keys.to_pubkey() == pk && sk == keys;
    }

#ifdef HAVE_CRYPT
    bool check_passwd_hash(std::string pwhash, std::string challenge)
    {
        bool ret = false;
        auto pos = pwhash.find_last_of('$');
        auto settings = pwhash.substr(0, pos);
        crypt_data data{};
        if (char* ptr = crypt_r(challenge.c_str(), settings.c_str(), &data))
        {
            ret = ptr == pwhash;
        }
        sodium_memzero(&data, sizeof(data));
        return ret;
    }
#endif

    // Called during static initialization to initialize libsodium.  (The CSRNG return is
    // not useful, but just here to get this called during static initialization of `csrng`).
    static CSRNG _initialize_crypto()
    {
        if (sodium_init() == -1)
        {
            log::critical(logcat, "sodium_init() failed, unable to continue!");
            std::abort();
        }

        return CSRNG{};
    }

    CSRNG csrng = _initialize_crypto();
}  // namespace llarp::crypto
