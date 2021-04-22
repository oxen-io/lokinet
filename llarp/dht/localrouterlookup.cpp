#include "localrouterlookup.hpp"

#include "context.hpp"
#include <llarp/dht/messages/gotrouter.hpp>

#include <llarp/path/path_context.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/routing/dht_message.hpp>
#include <llarp/util/logging/logger.hpp>

namespace llarp
{
  namespace dht
  {
    LocalRouterLookup::LocalRouterLookup(
        const PathID_t& path, uint64_t txid, const RouterID& _target, AbstractContext* ctx)
        : RecursiveRouterLookup(TXOwner{ctx->OurKey(), txid}, _target, ctx, nullptr)
        , localPath(path)
    {}

    void
    LocalRouterLookup::SendReply()
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
      if (valuesFound.size())
      {
        RouterContact found;
        for (const auto& rc : valuesFound)
        {
          if (rc.OtherIsNewer(found))
            found = rc;
        }
        valuesFound.clear();
        if (not found.pubkey.IsZero())
        {
          valuesFound.resize(1);
          valuesFound[0] = found;
        }
        else
        {
          llarp::LogWarn("We found a null RC for dht request, dropping it");
        }
      }
      routing::DHTMessage msg;
      msg.M.emplace_back(new GotRouterMessage(parent->OurKey(), whoasked.txid, valuesFound, true));
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
