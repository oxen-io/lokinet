#include "recursiverouterlookup.hpp"

#include "context.hpp"
#include <llarp/dht/messages/findrouter.hpp>
#include <llarp/dht/messages/gotrouter.hpp>

#include <llarp/router/abstractrouter.hpp>
#include <llarp/router/i_rc_lookup_handler.hpp>

#include <utility>

namespace llarp
{
  namespace dht
  {
    RecursiveRouterLookup::RecursiveRouterLookup(
        const TXOwner& _whoasked,
        const RouterID& _target,
        AbstractContext* ctx,
        RouterLookupHandler result)
        : TX<RouterID, RouterContact>(_whoasked, _target, ctx), resultHandler(std::move(result))

    {
      peersAsked.insert(ctx->OurKey());
    }

    bool
    RecursiveRouterLookup::Validate(const RouterContact& rc) const
    {
      if (!rc.Verify(parent->Now()))
      {
        llarp::LogWarn("rc from lookup result is invalid");
        return false;
      }
      return true;
    }

    void
    RecursiveRouterLookup::Start(const TXOwner& peer)
    {
      parent->DHTSendTo(peer.node.as_array(), new FindRouterMessage(peer.txid, target));
    }

    void
    RecursiveRouterLookup::SendReply()
    {
      if (valuesFound.size())
      {
        RouterContact found;
        for (const auto& rc : valuesFound)
        {
          if (found.OtherIsNewer(rc) && parent->GetRouter()->rcLookupHandler().CheckRC(rc))
            found = rc;
        }
        valuesFound.clear();
        valuesFound.emplace_back(found);
      }
      if (resultHandler)
      {
        resultHandler(valuesFound);
      }
      if (whoasked.node != parent->OurKey())
      {
        parent->DHTSendTo(
            whoasked.node.as_array(),
            new GotRouterMessage({}, whoasked.txid, valuesFound, false),
            false);
      }
    }
  }  // namespace dht
}  // namespace llarp
