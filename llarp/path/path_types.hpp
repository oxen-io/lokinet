#ifndef LLARP_PATH_TYPES_HPP
#define LLARP_PATH_TYPES_HPP

#include <crypto/crypto.hpp>
#include <util/aligned.hpp>

namespace llarp
{
  using PathID_t = AlignedBuffer< PATHIDSIZE >;
}

#endif
