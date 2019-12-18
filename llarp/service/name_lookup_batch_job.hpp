#ifndef LLARP_SERVICE_NAME_LOOKUP_BATCH_JOB_HPP
#define LLARP_SERVICE_NAME_LOOKUP_BATCH_JOB_HPP

#include <service/lookup.hpp>
#include <naming/consensus_algorithm.hpp>
#include <naming/i_name_lookup_handler.hpp>
#include <absl/types/optional.h>

namespace llarp
{
  namespace service
  {
    struct Endpoint;

    struct NameLookupBatchJob
        : public std::enable_shared_from_this< NameLookupBatchJob >
    {
      static constexpr llarp_time_t Lifetime = 60 * 1000;

      std::string Name;
      naming::ConsensusAlgorithm< Address > consensusAlgo;
      naming::NameLookupResultHandler handler;
      std::unordered_map< RouterID, absl::optional< Address >, RouterID::Hash >
          results;

      std::unordered_map< Address, IntroSet, Address::Hash > introsets;

      Endpoint* endpoint       = nullptr;
      llarp_time_t expiresAt   = 0;
      size_t m_NumRequestsMade = 0;

      bool
      MakeRequest(const path::Path_ptr& path, uint64_t txid);

      void
      AddResponse(const RouterID ep, absl::optional< IntroSet > result);

      bool
      HandleResult();

      bool
      HasEnoughVotes() const;
    };
  }  // namespace service
}  // namespace llarp
#endif
