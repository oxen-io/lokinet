#pragma once

#include <llarp/crypto/constants.hpp>
#include <llarp/util/aligned.hpp>

namespace llarp
{
  struct PathID_t final : public AlignedBuffer<PATHIDSIZE>
  {
    using Hash = AlignedBuffer<PATHIDSIZE>::Hash;
  };

}  // namespace llarp
