#include <path.hpp>
#include <service/endpoint.hpp>
#include <service/lookup.hpp>
#include <util/time.hpp>

namespace llarp
{
  namespace service
  {
    IServiceLookup::IServiceLookup(ILookupHolder *p, uint64_t tx,
                                   const std::string &n)
        : parent(p), txid(tx), name(n)
    {
      m_created = time_now_ms();
      p->PutLookup(this, tx);
    }

    bool
    IServiceLookup::SendRequestViaPath(llarp::path::Path *path,
                                       llarp::Router *r)
    {
      auto msg = BuildRequestMessage();
      if(!msg)
        return false;
      auto result = path->SendRoutingMessage(msg, r);
      endpoint    = path->Endpoint();
      delete msg;
      return result;
    }
  }  // namespace service
}  // namespace llarp
