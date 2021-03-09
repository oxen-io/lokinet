#pragma once

#include <llarp/crypto/types.hpp>
#include <vector>

namespace llarp
{
  namespace consensus
  {
    /// consensus table
    struct Table : public std::vector<RouterID>
    {
      ShortHash
      CalculateHash() const;
    };
  }  // namespace consensus
}  // namespace llarp
