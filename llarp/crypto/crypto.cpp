#include "crypto.hpp"

#include <llarp/crypto/keys.hpp>
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
#include <stdexcept>
#ifdef LOKINET_HAVE_CRYPT
#include <crypt.h>
#endif

namespace llarp::crypto
{
    static auto logcat = log::Cat("crypto");

    static_assert(MAC_SIZE == crypto_aead_xchacha20poly1305_ietf_ABYTES);
    static_assert(SymmNonce::SIZE == crypto_stream_xchacha20_NONCEBYTES);
    static_assert(SharedSecret::SIZE == crypto_stream_xchacha20_KEYBYTES);

    static bool dh(
        SharedSecret& out,
        const PubKey& client_pk,
        const PubKey& server_pk,
        bool we_are_client,
        const Ed25519SecretKey& local_keys,
        const SymmNonce& nonce)
    {
        SharedSecret shared;

        // Somewhat misnamed: actually gets the private scalar (which happens to be what you need
        // for converting to X):
        std::array<unsigned char, 32> a;
        crypto_sign_ed25519_sk_to_curve25519(a.data(), local_keys.data());

        if (crypto_scalarmult_ed25519(shared.data(), a.data(), (we_are_client ? server_pk : client_pk).data()))
            return false;

        crypto_generichash_blake2b_state h;
        crypto_generichash_blake2b_init(&h, nonce.data(), nonce.size(), shared.size());
        crypto_generichash_blake2b_update(&h, client_pk.data(), client_pk.size());
        crypto_generichash_blake2b_update(&h, server_pk.data(), server_pk.size());
        crypto_generichash_blake2b_update(&h, shared.data(), shared.size());
        crypto_generichash_blake2b_final(&h, out.data(), out.size());
        return true;
    }

    std::optional<RouterID> maybe_decrypt_name(std::string_view ciphertext, SymmNonce nonce, std::string_view namestr)
    {
        const auto payloadsize = ciphertext.size() - MAC_SIZE;
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

    void xchacha20(std::span<std::byte> buf, const SharedSecret& secret, const SymmNonce& nonce)
    {
        auto* d = reinterpret_cast<unsigned char*>(buf.data());
        crypto_stream_xchacha20_xor(d, d, buf.size(), nonce.data(), secret.data());
    }

    void xchacha20_poly1305_encrypt(std::span<std::byte> buf, const SharedSecret& secret, const SymmNonce& nonce)
    {
        if (buf.size() <= MAC_SIZE)
        {
            const auto err = fmt::format("Payload size {} is <= poly1305 AEAD size ({})!", buf.size(), MAC_SIZE);
            log::error(logcat, "{}", err);
            throw std::invalid_argument{err};
        }
        auto payload_size = buf.size() - MAC_SIZE;
        auto* buf_cptr = reinterpret_cast<unsigned char*>(buf.data());
        crypto_aead_xchacha20poly1305_ietf_encrypt(
            buf_cptr, nullptr, buf_cptr, payload_size, nullptr, 0, nullptr, nonce.data(), secret.data());
    }

    void xchacha20_poly1305_encrypt(std::string& buf, const SharedSecret& secret, const SymmNonce& nonce)
    {
        xchacha20_poly1305_encrypt(
            std::span<std::byte>{reinterpret_cast<std::byte*>(buf.data()), buf.size()}, secret, nonce);
    }

    std::span<std::byte> xchacha20_poly1305_decrypt(
        std::span<std::byte> buf, const SharedSecret& secret, const SymmNonce& nonce)
    {
        if (buf.size() <= MAC_SIZE)
        {
            log::warning(logcat, "On decryption, payload size {} is < poly1305 AEAD size ({})!", buf.size(), MAC_SIZE);
            return {};
        }
        auto* buf_cptr = reinterpret_cast<unsigned char*>(buf.data());
        unsigned long long payload_size{0};
        if (crypto_aead_xchacha20poly1305_ietf_decrypt(
                buf_cptr, &payload_size, nullptr, buf_cptr, buf.size(), nullptr, 0, nonce.data(), secret.data())
            != 0)
        {
            log::warning(logcat, "On decryption, payload failed authentication!");
            return {};
        }
        assert(payload_size == buf.size() - MAC_SIZE);
        return buf.subspan(0, payload_size);
    }

    bool dh_client(SharedSecret& shared, const PubKey& pk, const Ed25519SecretKey& sk, const SymmNonce& n)
    {
        if (dh(shared, sk.to_pubkey(), pk, true, sk, n))
            return true;

        log::warning(logcat, "dh_client - dh failed");
        return false;
    }

    std::tuple<SharedSecret, PubKey, SymmNonce> dh_client_gen(const PubKey& server_pk)
    {
        std::tuple<SharedSecret, PubKey, SymmNonce> result;
        auto& [secret, eph_pk, nonce] = result;

        auto eph_keys = generate_ed25519();
        nonce = SymmNonce::make_random();
        if (!dh_client(secret, server_pk, eph_keys, nonce))
            throw std::invalid_argument{"shared secret generation failed: remote pubkey is not a valid Ed25519 pubkey"};
        eph_pk.assign(eph_keys.pubkey_span());
        return result;
    }

    /// path dh relay side
    bool dh_server(SharedSecret& shared, const PubKey& pk, const Ed25519SecretKey& sk, const SymmNonce& n)
    {
        if (dh(shared, pk, sk.to_pubkey(), false, sk, n))
            return true;

        log::warning(logcat, "dh_server - dh failed");
        return false;
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

    bool verify(
        std::span<const std::byte, PUBKEYSIZE> pub,
        std::span<const std::byte> data,
        std::span<const std::byte, SIGSIZE> sig)
    {
        return crypto_sign_verify_detached(
                   as_uspan(sig).data(), as_uspan(data).data(), data.size(), as_uspan(pub).data())
            != -1;
    }

    // FIXME: the following two functions are nearly identical, but different in stupid ways
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
        xchacha20(payload, secret, nonce);
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
        xchacha20(encrypted, shared, nonce);

        log::trace(logcat, "Shared secret: {}", shared.to_string());
    }

    std::array<unsigned char, 32> blinding_scalar(std::span<const std::byte, 32> pubkey, std::string_view blind_domain)
    {
        if (blind_domain.size() > crypto_generichash_KEYBYTES_MAX)
            blind_domain = blind_domain.substr(0, crypto_generichash_KEYBYTES_MAX);

        // n = H(pk, key=blind_domain)
        std::array<unsigned char, 64> n;
        crypto_generichash_blake2b(
            n.data(),
            n.size(),
            reinterpret_cast<const unsigned char*>(pubkey.data()),
            pubkey.size(),
            reinterpret_cast<const unsigned char*>(blind_domain.data()),
            blind_domain.size());

        // out = scalar_reduce(n)
        std::array<unsigned char, 32> out;
        crypto_core_ed25519_scalar_reduce(out.data(), n.data());

        return out;
    }

    bool blind(PubKey& blinded, const PubKey& root, std::string_view blind_domain)
    {
        return 0
            == crypto_scalarmult_ed25519_noclamp(
                   blinded.data(), blinding_scalar(root, blind_domain).data(), root.data());
    }

    Ed25519SecretKey generate_ed25519()
    {
        Ed25519SecretKey ret{};
        PubKey pk;
        [[maybe_unused]] int result = crypto_sign_ed25519_keypair(pk.data(), ret.data());
        assert(result != -1);
        return ret;
    }

    bool check_pubkey(const Ed25519SecretKey& keys)
    {
        PubKey pk;
        Ed25519SecretKey sk;
        crypto_sign_seed_keypair(pk.data(), sk.data(), keys.data());
        return keys.to_pubkey() == pk;
    }

#ifdef LOKINET_HAVE_CRYPT
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

}  // namespace llarp::crypto

namespace llarp
{
    // Called during static initialization to initialize libsodium.  (The CSRNG return is
    // not useful, but just here to get this called during static initialization of `csrng`).
    static CSRNG _initialize_crypto()
    {
        if (sodium_init() == -1)
        {
            log::critical(crypto::logcat, "sodium_init() failed, unable to continue!");
            std::abort();
        }

        return CSRNG{};
    }

    CSRNG csrng = _initialize_crypto();

}  // namespace llarp
