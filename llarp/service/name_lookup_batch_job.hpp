#ifndef LLARP_SERVICE_NAME_LOOKUP_BATCH_JOB_HPP
#define LLARP_SERVICE_NAME_LOOKUP_BATCH_JOB_HPP

#include <service/lookup.hpp>

namespace llarp
{
  namespace service
  {
    struct NameLookupBatchJob
    {
      static constexpr llarp_time_t Lifetime = 60 * 1000;

      std::string Name;
      naming::NameLookupResultHandler handler;
      std::unordered_map< RouterID, Address, RouterID::Hash > results;

      llarp_time_t expiresAt = 0;

      bool
      MakeRequest(ILookupHolder* h, const path::Path_ptr& path, uint64_t txid);
    };
  }  // namespace service
}  // namespace llarp
#endif
