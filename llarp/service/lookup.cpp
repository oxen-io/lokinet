#include <service/lookup.hpp>

#include <path/path.hpp>
#include <util/time.hpp>

namespace llarp
{
  struct AbstractRouter;

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
    IServiceLookup::SendRequestViaPath(path::Path *path, AbstractRouter *r)
    {
      auto msg = BuildRequestMessage();
      if(!msg)
        return false;
      auto result = path->SendRoutingMessage(*msg, r);
      endpoint    = path->Endpoint();
      return result;
    }
  }  // namespace service
}  // namespace llarp
