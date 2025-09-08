#include "types.hpp"

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

}  // namespace llarp
