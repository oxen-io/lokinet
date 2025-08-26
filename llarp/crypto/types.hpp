#pragma once

#include "constants.hpp"

#include <llarp/util/aligned.hpp>

#include <algorithm>

namespace llarp
{
    using SharedSecret = AlignedBuffer<SHAREDKEYSIZE>;

    struct Signature final : public AlignedBuffer<SIGSIZE>
    {};

    struct SymmNonce final : public AlignedBuffer<NONCESIZE>
    {
        using AlignedBuffer<NONCESIZE>::AlignedBuffer;

        SymmNonce operator^(const SymmNonce& other) const
        {
            SymmNonce ret;
            std::transform(begin(), end(), other.begin(), ret.begin(), std::bit_xor<>());
            return ret;
        }

        static SymmNonce make_random();

        // Creates a SymmNonce from a sequential value, encoded in little endian.  This value should
        // never be reused!  This is intended for use with established sessions with an incrementing
        // nonce.  The mid and high values can be specified, typically for flags for sequential
        // nonce distinction (for instance: sessions set mid=1 for messages on inbound paths, and
        // mid=0 for messages on outbound paths).
        static SymmNonce sequential(uint64_t low, uint64_t mid = 0, uint64_t high = 0);
    };

}  // namespace llarp
