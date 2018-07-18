#include <llarp/path.hpp>
#include <llarp/service/lookup.hpp>
namespace llarp
{
  namespace service
  {
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