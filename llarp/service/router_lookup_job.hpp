#pragma once

#include <llarp/router_contact.hpp>

namespace llarp
{
  namespace service
  {
    struct Endpoint;

    struct RouterLookupJob
    {
      RouterLookupJob(Endpoint* p, RouterLookupHandler h);

      RouterLookupHandler handler;
      uint64_t txid;
      llarp_time_t started;

      bool
      IsExpired(llarp_time_t now) const
      {
        if (now < started)
          return false;
        return now - started > 30s;
      }

      void
      InformResult(std::vector<RouterContact> result)
      {
        if (handler)
          handler(result);
      }
    };
  }  // namespace service
}  // namespace llarp
