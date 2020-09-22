#ifndef LLARP_CONSENSUS_TABLE_HPP
#define LLARP_CONSENSUS_TABLE_HPP

#include <crypto/types.hpp>
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

#endif
