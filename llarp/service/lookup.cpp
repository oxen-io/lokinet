#include <llarp/path.hpp>
#include <llarp/service/endpoint.hpp>
#include <llarp/service/lookup.hpp>

namespace llarp
{
  namespace service
  {
    IServiceLookup::IServiceLookup(ILookupHolder *p, uint64_t tx)
        : parent(p), txid(tx)
    {
      p->PutLookup(this, tx);
    }

    bool
    IServiceLookup::SendRequestViaPath(llarp::path::Path *path, llarp_router *r)
    {
      auto msg = BuildRequestMessage();
      if(!msg)
        return false;
      auto result = path->SendRoutingMessage(msg, r);
      delete msg;
      return result;
    }
  }  // namespace service
}  // namespace llarp