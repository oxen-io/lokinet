#include "publishservicejob.hpp"

#include "context.hpp"
#include <llarp/dht/messages/pubintro.hpp>
#include <llarp/dht/messages/gotintro.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/routing/path_dht_message.hpp>
#include <llarp/router/router.hpp>

#include <utility>
namespace llarp::dht
{
  PublishServiceJob::PublishServiceJob(
      const TXOwner& asker,
      const service::EncryptedIntroSet& introset_,
      AbstractDHTMessageHandler* ctx,
      uint64_t relayOrder_)
      : TX<TXOwner, service::EncryptedIntroSet>(asker, asker, ctx)
      , relayOrder(relayOrder_)
      , introset(introset_)
  {}

  bool
  PublishServiceJob::Validate(const service::EncryptedIntroSet& value) const
  {
    if (value.derivedSigningKey != introset.derivedSigningKey)
    {
      llarp::LogWarn("publish introset acknowledgement acked a different service");
      return false;
    }
    const llarp_time_t now = llarp::time_now_ms();
    return value.Verify(now);
  }

  void
  PublishServiceJob::Start(const TXOwner& peer)
  {
    parent->DHTSendTo(
        peer.node.as_array(), new PublishIntroMessage(introset, peer.txid, false, relayOrder));
  }

  void
  PublishServiceJob::SendReply()
  {
    parent->DHTSendTo(whoasked.node.as_array(), new GotIntroMessage({introset}, whoasked.txid));
  }

  LocalPublishServiceJob::LocalPublishServiceJob(
      const TXOwner& peer,
      const PathID_t& fromID,
      uint64_t _txid,
      const service::EncryptedIntroSet& introset,
      AbstractDHTMessageHandler* ctx,
      uint64_t relayOrder)
      : PublishServiceJob(peer, introset, ctx, relayOrder), localPath(fromID), txid(_txid)
  {}

  void
  LocalPublishServiceJob::SendReply()
  {
    auto path =
        parent->GetRouter()->path_context().GetByUpstream(parent->OurKey().as_array(), localPath);
    if (!path)
    {
      llarp::LogWarn(
          "did not send reply for relayed dht request, no such local path "
          "for pathid=",
          localPath);
      return;
    }
    routing::PathDHTMessage msg;
    msg.dht_msgs.emplace_back(new GotIntroMessage({introset}, txid));
    if (!path->SendRoutingMessage(msg, parent->GetRouter()))
    {
      llarp::LogWarn(
          "failed to send routing message when informing result of dht "
          "request, pathid=",
          localPath);
    }
  }
}  // namespace llarp::dht
