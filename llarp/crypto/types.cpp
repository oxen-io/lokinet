#include "types.hpp"

#include "oxenc/endian.h"

#include <llarp/contact/relay_contact.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/file.hpp>
#include <llarp/util/logging.hpp>

#include <oxenc/base32z.h>
#include <oxenc/hex.h>
#include <sodium/crypto_core_ed25519.h>
#include <sodium/crypto_generichash.h>
#include <sodium/crypto_hash_sha512.h>
#include <sodium/crypto_scalarmult_ed25519.h>

namespace llarp
{
    static auto logcat = log::Cat("cryptoutils");

    PubKey Ed25519SecretKey::to_pubkey() const { return PubKey{span().last<32>()}; }

    bool Ed25519SecretKey::load_from_file(const fs::path& fname)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        size_t sz;
        std::string tmp;
        tmp.resize(128);

        try
        {
            sz = util::file_to_buffer(fname, tmp.data(), tmp.size());
        }
        catch (const std::exception& e)
        {
            log::critical(logcat, "Failed to read contents from file: {}", e.what());
            return false;
        }

        std::copy_n(tmp.begin(), sz, begin());
        return true;
    }

    bool Ed25519SecretKey::recalculate()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        Ed25519PrivateData key = to_eddata();
        PubKey pubkey = key.to_pubkey();
        std::memcpy(data() + 32, pubkey.data(), 32);
        return true;
    }

    Ed25519PrivateData Ed25519SecretKey::to_eddata() const
    {
        Ed25519PrivateData k;
        unsigned char h[crypto_hash_sha512_BYTES];
        crypto_hash_sha512(h, data(), 32);
        h[0] &= 248;
        h[31] &= 63;
        h[31] |= 64;
        std::memcpy(k.data(), h, 64);
        return k;
    }

    Ed25519PrivateData Ed25519SecretKey::derive_private_subkey_data(uint64_t domain) const
    {
        Ed25519PrivateData ret{};

        std::array<unsigned char, 32> h = crypto::make_scalar(to_pubkey(), domain);

        auto a = to_eddata();

        // a' = ha
        crypto_core_ed25519_scalar_mul(ret.data(), h.data(), a.data());

        // s' = H(h || s)
        std::array<uint8_t, 64> buf;
        std::copy(h.begin(), h.end(), buf.begin());
        std::copy(a.signing_hash().begin(), a.signing_hash().end(), buf.begin() + 32);
        if (crypto_generichash_blake2b(ret.signing_hash().data(), 32, buf.data(), buf.size(), nullptr, 0) == -1)
            throw std::runtime_error{"Call to `crypto_generichash_blake2b` failed!"};
        return ret;
    }

    PubKey Ed25519PrivateData::to_pubkey() const
    {
        PubKey p;
        crypto_scalarmult_ed25519_base_noclamp(p.data(), data());
        return p;
    }

    bool Ed25519SecretKey::write_to_file(const fs::path& fname) const
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        try
        {
            util::buffer_to_file(fname, to_view());
        }
        catch (const std::exception& e)
        {
            log::critical(logcat, "Failed to write contents to file: {}", e.what());
            return false;
        }

        return true;
    }

    SymmNonce SymmNonce::make_random()
    {
        SymmNonce n;
        randombytes_buf(n.data(), n.size());
        return n;
    }

    SymmNonce SymmNonce::sequential(uint64_t low, uint64_t mid, uint64_t high)
    {
        SymmNonce n;
        oxenc::write_host_as_little(low, n.data());
        oxenc::write_host_as_little(mid, n.data() + 8);
        oxenc::write_host_as_little(high, n.data() + 16);
        static_assert(n.SIZE == 24);
        return n;
    }

    hash_key hash_key::derive_from_rid(PubKey root)
    {
        hash_key derived;
        crypto::derive_subkey(derived.data(), derived.size(), root, 1);
        return derived;
    }

    std::string hash_key::to_string() const { return oxenc::to_base32z(begin(), end()); }

    bool XorMetric::operator()(const hash_key& left, const hash_key& right) const { return (us ^ left) < (us ^ right); }

    bool XorMetric::operator()(const RemoteRC& left, const RemoteRC& right) const
    {
        return (left.router_id() ^ us) < (right.router_id() ^ us);
    }

}  // namespace llarp
