#include <dht/localtaglookup.hpp>

#include <dht/context.hpp>
#include <dht/messages/gotintro.hpp>
#include <messages/dht.hpp>
#include <router/router.hpp>

namespace llarp
{
  namespace dht
  {
    LocalTagLookup::LocalTagLookup(const PathID_t &path, uint64_t txid,
                                   const service::Tag &target, Context *ctx)
        : TagLookup(TXOwner{ctx->OurKey(), txid}, target, ctx, 0)
        , localPath(path)
    {
    }

    void
    LocalTagLookup::SendReply()
    {
      auto path = parent->router->paths.GetByUpstream(
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
      msg.M.emplace_back(new GotIntroMessage(valuesFound, whoasked.txid));
      if(!path->SendRoutingMessage(&msg, parent->router))
      {
        llarp::LogWarn(
            "failed to send routing message when informing result of dht "
            "request, pathid=",
            localPath);
      }
    }
  }  // namespace dht
}  // namespace llarp
