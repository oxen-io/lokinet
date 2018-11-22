#ifndef LLARP_PATH_TYPES_HPP
#define LLARP_PATH_TYPES_HPP

#include <llarp/crypto.h>
#include <llarp/aligned.hpp>

namespace llarp
{
  using PathID_t = AlignedBuffer< PATHIDSIZE >;
}

#endif
