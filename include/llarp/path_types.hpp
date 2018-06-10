#ifndef LLARP_PATH_TYPES_HPP
#define LLARP_PATH_TYPES_HPP

#include <llarp/aligned.hpp>

namespace llarp
{
  typedef AlignedBuffer< 16 > PathID_t;

  typedef AlignedBuffer< 32 > PathSymKey_t;

  typedef AlignedBuffer< 32 > PathNonce_t;

  typedef AlignedBuffer< 24 > SymmNonce_t;
}

#endif