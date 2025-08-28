#include "types.hpp"

#include "oxenc/endian.h"

#include <llarp/util/logging.hpp>

#include <sodium/randombytes.h>

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
