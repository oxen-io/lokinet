#include <dht/localrouterlookup.hpp>

#include <dht/context.hpp>
#include <dht/messages/gotrouter.hpp>
#include <messages/dht.hpp>
#include <router/router.hpp>
#include <util/logger.hpp>

namespace llarp
{
  namespace dht
  {
    LocalRouterLookup::LocalRouterLookup(const PathID_t &path, uint64_t txid,
                                         const RouterID &target,
                                         AbstractContext *ctx)
        : RecursiveRouterLookup(TXOwner{ctx->OurKey(), txid}, target, ctx,
                                nullptr)
        , localPath(path)
    {
    }

    void
    LocalRouterLookup::SendReply()
    {
      auto path = parent->GetRouter()->paths.GetByUpstream(
          parent->OurKey().as_array(), localPath);
      if(!path)
      {
        llarp::LogWarn(
            "did not send reply for relayed dht request, no such local path "
            "for pathid=",
            localPath);
        return;
      }
      routing::DHTMessage msg;
      msg.M.emplace_back(new GotRouterMessage(parent->OurKey(), whoasked.txid,
                                              valuesFound, true));
      if(!path->SendRoutingMessage(&msg, parent->GetRouter()))
      {
        llarp::LogWarn(
            "failed to send routing message when informing result of dht "
            "request, pathid=",
            localPath);
      }
    }
  }  // namespace dht
}  // namespace llarp
