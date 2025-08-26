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
#include <sodium/crypto_sign.h>

namespace llarp
{
    static auto logcat = log::Cat("cryptoutils");

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

}  // namespace llarp
