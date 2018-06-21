#ifndef LLARP_PATH_TYPES_HPP
#define LLARP_PATH_TYPES_HPP

#include <llarp/crypto.h>
#include <llarp/aligned.hpp>

namespace llarp
{
  typedef AlignedBuffer< PATHIDSIZE > PathID_t;
}

#endif