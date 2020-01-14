#include <dht/recursiverouterlookup.hpp>

#include <dht/context.hpp>
#include <dht/messages/findrouter.hpp>
#include <dht/messages/gotrouter.hpp>
#include <utility>

namespace llarp
{
  namespace dht
  {
    RecursiveRouterLookup::RecursiveRouterLookup(const TXOwner &_whoasked,
                                                 const RouterID &_target,
                                                 AbstractContext *ctx,
                                                 RouterLookupHandler result)
        : TX< RouterID, RouterContact >(_whoasked, _target, ctx)
        , resultHandler(std::move(result))

    {
      peersAsked.insert(ctx->OurKey());
    }

    bool
    RecursiveRouterLookup::Validate(const RouterContact &rc) const
    {
      if(!rc.Verify(parent->Now()))
      {
        llarp::LogWarn("rc from lookup result is invalid");
        return false;
      }
      return true;
    }

    bool
    RecursiveRouterLookup::GetNextPeer(Key_t &nextPeer,
                                       const std::set< Key_t > &exclude)
    {
      const Key_t K(target.as_array());
      return parent->Nodes()->FindCloseExcluding(K, nextPeer, exclude);
    }

    void
    RecursiveRouterLookup::DoNextRequest(const Key_t &peer)
    {
      peersAsked.emplace(peer);
      parent->LookupRouterRecursive(target, whoasked.node, whoasked.txid, peer,
                                    resultHandler);
    }

    void
    RecursiveRouterLookup::Start(const TXOwner &peer)
    {
      peersAsked.emplace(peer.node);
      parent->DHTSendTo(peer.node.as_array(),
                        new FindRouterMessage(peer.txid, target));
    }

    void
    RecursiveRouterLookup::SendReply()
    {
      if(valuesFound.size())
      {
        RouterContact found;
        for(const auto &rc : valuesFound)
        {
          if(found.OtherIsNewer(rc))
            found = rc;
        }
        valuesFound.clear();
        valuesFound.emplace_back(found);
      }
      if(resultHandler)
      {
        resultHandler(valuesFound);
      }
      else if(whoasked.node != parent->OurKey())
      {
        parent->DHTSendTo(
            whoasked.node.as_array(),
            new GotRouterMessage({}, whoasked.txid, valuesFound, false), false);
      }
      // store this in our nodedb for caching
      if(valuesFound.size() > 0)
        parent->StoreRC(valuesFound[0]);
    }
  }  // namespace dht
}  // namespace llarp
