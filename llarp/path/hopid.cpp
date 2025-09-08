#include "hopid.hpp"

#include <sodium/randombytes.h>

namespace llarp
{
    HopID HopID::make_random()
    {
        HopID h;
        randombytes_buf(h.data(), h.size());
        return h;
    }
}  // namespace llarp
