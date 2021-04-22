#include "router_lookup_job.hpp"

#include "endpoint.hpp"
#include <utility>

namespace llarp
{
  namespace service
  {
    RouterLookupJob::RouterLookupJob(Endpoint* p, RouterLookupHandler h)
        : handler(std::move(h)), txid(p->GenTXID()), started(p->Now())
    {}

  }  // namespace service
}  // namespace llarp
