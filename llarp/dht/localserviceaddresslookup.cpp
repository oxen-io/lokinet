#include "localserviceaddresslookup.hpp"

#include "context.hpp"
#include <llarp/dht/messages/gotintro.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/router/router.hpp>
#include <llarp/routing/path_dht_message.hpp>
#include <llarp/util/logging.hpp>

namespace llarp::dht
{
  LocalServiceAddressLookup::LocalServiceAddressLookup(
      const PathID_t& pathid,
      uint64_t txid,
      uint64_t relayOrder,
      const Key_t& addr,
      AbstractDHTMessageHandler* ctx,
      [[maybe_unused]] const Key_t& askpeer)
      : ServiceAddressLookup(TXOwner{ctx->OurKey(), txid}, addr, ctx, relayOrder, nullptr)
      , localPath(pathid)
  {}

  void
  LocalServiceAddressLookup::SendReply()
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
    // pick newest if we have more than 1 result
    if (valuesFound.size())
    {
      service::EncryptedIntroSet found;
      for (const auto& introset : valuesFound)
      {
        if (found.OtherIsNewer(introset))
          found = introset;
      }
      valuesFound.clear();
      valuesFound.emplace_back(found);
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