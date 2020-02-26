#include <service/lookup.hpp>

#include <path/path.hpp>
#include <util/time.hpp>
#include <router/abstractrouter.hpp>
#include <util/thread/logic.hpp>
#include <utility>

namespace llarp
{
  struct AbstractRouter;

  namespace service
  {
    IServiceLookup::IServiceLookup(uint64_t tx, std::string n)
        : txid(tx), name(std::move(n)), m_created(time_now_ms())
    {
    }

    bool
    IServiceLookup::SendRequestViaPath(path::Path_ptr path, AbstractRouter *r)
    {
      auto msg = BuildRequestMessage();
      if(!msg)
        return false;
      endpoint = path->Endpoint();
      return path->SendRoutingMessage(*msg, r);
    }
  }  // namespace service
}  // namespace llarp
