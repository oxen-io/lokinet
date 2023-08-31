#include "localtaglookup.hpp"

#include "context.hpp"
#include <llarp/dht/messages/gotintro.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/routing/path_dht_message.hpp>

namespace llarp::dht
{
  LocalTagLookup::LocalTagLookup(
      const PathID_t& path, uint64_t txid, const service::Tag& _target, AbstractContext* ctx)
      : TagLookup(TXOwner{ctx->OurKey(), txid}, _target, ctx, 0), localPath(path)
  {}

  void
  LocalTagLookup::SendReply()
  {
    auto path =
        parent->GetRouter()->pathContext().GetByUpstream(parent->OurKey().as_array(), localPath);
    if (!path)
    {
      llarp::LogWarn(
          "did not send reply for relayed dht request, no such local path "
          "for pathid=",
          localPath);
      return;
    }
    routing::PathDHTMessage msg;
    msg.dht_msgs.emplace_back(new GotIntroMessage(valuesFound, whoasked.txid));
    if (!path->SendRoutingMessage(msg, parent->GetRouter()))
    {
      llarp::LogWarn(
          "failed to send routing message when informing result of dht "
          "request, pathid=",
          localPath);
    }
  }
}  // namespace llarp::dht
