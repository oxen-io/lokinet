#include "lookup.hpp"

#include <llarp/path/path.hpp>
#include <llarp/util/time.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <utility>

namespace llarp
{
  struct AbstractRouter;

  namespace service
  {
    IServiceLookup::IServiceLookup(
        ILookupHolder* p, uint64_t tx, std::string n, llarp_time_t timeout)
        : m_parent(p), txid(tx), name(std::move(n)), m_created{time_now_ms()}, m_timeout{timeout}
    {
      p->PutLookup(this, tx);
    }

    bool
    IServiceLookup::SendRequestViaPath(path::Path_ptr path, AbstractRouter* r)
    {
      auto msg = BuildRequestMessage();
      if (!msg)
        return false;
      r->loop()->call(
          [path = std::move(path), msg = std::move(msg), r] { path->SendRoutingMessage(*msg, r); });
      return true;
    }
  }  // namespace service
}  // namespace llarp
