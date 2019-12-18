#ifndef LLARP_NAMING_CONSENSUS_ALGORITHM_HPP
#define LLARP_NAMING_CONSENSUS_ALGORITHM_HPP
#include <unordered_map>
#include <absl/types/optional.h>

namespace llarp
{
  namespace naming
  {
    template < typename Val_t, typename Count_t = uint16_t,
               typename Vote_t  = std::pair< Val_t, Count_t >,
               typename Hash_t  = typename Val_t::Hash,
               typename Votes_t = std::unordered_map< Val_t, Count_t, Hash_t > >
    struct ConsensusAlgorithm
    {
      absl::optional< Val_t >
      ExtractAgreement(Votes_t votes, const size_t numCast) const
      {
        Vote_t chosen = {};
        for(const auto& item : votes)
        {
          if(item.second > chosen.second)
          {
            chosen = item;
          }
        }
        if(numCast - chosen.second > DisagreementThreshold)
        {
          // disagreement
          return {};
        }
        // agreement
        return chosen.first;
      }

      /// how many disagreements are permitted
      Count_t DisagreementThreshold = 0;
    };
  }  // namespace naming
}  // namespace llarp

#endif
