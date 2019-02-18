#ifndef LLARP_PATH_TYPES_HPP
#define LLARP_PATH_TYPES_HPP

#include <crypto/constants.hpp>
#include <util/aligned.hpp>

namespace llarp
{
  struct PathID_t final : public AlignedBuffer< PATHIDSIZE >
  {
  };

}  // namespace llarp

#endif
