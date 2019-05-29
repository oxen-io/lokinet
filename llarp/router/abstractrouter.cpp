#include <router/abstractrouter.hpp>
#include <nodedb.hpp>

namespace llarp
{
  AbstractRouter::~AbstractRouter()
  {
  }

  void
  AbstractRouter::EnsureRouter(RouterID router, RouterLookupHandler handler)
  {
    std::vector< RouterContact > found(1);
    if(nodedb()->Get(router, found[0]))
      handler(found);
    else
      LookupRouter(router, handler);
  }
}  // namespace llarp
