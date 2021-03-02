#include <service/lookup.hpp>

#include <path/path.hpp>
#include <util/time.hpp>
#include <router/abstractrouter.hpp>
#include <utility>

namespace llarp
{
  struct AbstractRouter;

  namespace service
  {
    IServiceLookup::IServiceLookup(ILookupHolder* p, uint64_t tx, std::string n)
        : m_parent(p), txid(tx), name(std::move(n))
    {
      m_created = time_now_ms();
      p->PutLookup(this, tx);
    }

    bool
    IServiceLookup::SendRequestViaPath(path::Path_ptr path, AbstractRouter* r)
    {
      auto msg = BuildRequestMessage();
      if (!msg)
        return false;
      r->loop()->call([path=std::move(path), msg=std::move(msg), r] { path->SendRoutingMessage(*msg, r); });
      return true;
    }
  }  // namespace service
}  // namespace llarp
