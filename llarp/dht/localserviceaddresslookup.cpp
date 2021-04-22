#include "localserviceaddresslookup.hpp"

#include "context.hpp"
#include <llarp/dht/messages/gotintro.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/routing/dht_message.hpp>
#include <llarp/util/logging/logger.hpp>

namespace llarp
{
  namespace dht
  {
    LocalServiceAddressLookup::LocalServiceAddressLookup(
        const PathID_t& pathid,
        uint64_t txid,
        uint64_t relayOrder,
        const Key_t& addr,
        AbstractContext* ctx,
        [[maybe_unused]] const Key_t& askpeer)
        : ServiceAddressLookup(TXOwner{ctx->OurKey(), txid}, addr, ctx, relayOrder, nullptr)
        , localPath(pathid)
    {}

    void
    LocalServiceAddressLookup::SendReply()
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
      routing::DHTMessage msg;
      msg.M.emplace_back(new GotIntroMessage(valuesFound, whoasked.txid));
      if (!path->SendRoutingMessage(msg, parent->GetRouter()))
      {
        llarp::LogWarn(
            "failed to send routing message when informing result of dht "
            "request, pathid=",
            localPath);
      }
    }
  }  // namespace dht
}  // namespace llarp
