#include <dht/recursiverouterlookup.hpp>

#include <dht/context.hpp>
#include <dht/messages/findrouter.hpp>
#include <dht/messages/gotrouter.hpp>

namespace llarp
{
  namespace dht
  {
    RecursiveRouterLookup::RecursiveRouterLookup(const TXOwner &whoasked,
                                                 const RouterID &target,
                                                 Context *ctx,
                                                 RouterLookupHandler result)
        : TX< RouterID, RouterContact >(whoasked, target, ctx)
        , resultHandler(result)

    {
      peersAsked.insert(ctx->OurKey());
    }

    bool
    RecursiveRouterLookup::Validate(const RouterContact &rc) const
    {
      if(!rc.Verify(parent->Crypto(), parent->Now()))
      {
        llarp::LogWarn("rc from lookup result is invalid");
        return false;
      }
      return true;
    }

    void
    RecursiveRouterLookup::Start(const TXOwner &peer)
    {
      parent->DHTSendTo(peer.node.as_array(),
                        new FindRouterMessage(peer.txid, target));
    }

    void
    RecursiveRouterLookup::SendReply()
    {
      if(resultHandler)
      {
        resultHandler(valuesFound);
      }
      else
      {
        parent->DHTSendTo(
            whoasked.node.as_array(),
            new GotRouterMessage({}, whoasked.txid, valuesFound, false));
      }
    }
  }  // namespace dht
}  // namespace llarp
