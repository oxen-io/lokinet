#include <dht/explorenetworkjob.hpp>

#include <dht/context.hpp>
#include <dht/messages/findrouter.hpp>
#include <router/abstractrouter.hpp>

#include <nodedb.hpp>

#include <tooling/dht_event.hpp>

namespace llarp
{
  namespace dht
  {
    void
    ExploreNetworkJob::Start(const TXOwner& peer)
    {
      auto msg = new FindRouterMessage(peer.txid);
      auto router = parent->GetRouter();
      router->NotifyRouterEvent<tooling::FindRouterSentEvent>(router->pubkey(), msg);

      parent->DHTSendTo(peer.node.as_array(), msg);
    }

    void
    ExploreNetworkJob::SendReply()
    {
      llarp::LogDebug("got ", valuesFound.size(), " routers from exploration");

      auto router = parent->GetRouter();
      using std::placeholders::_1;
      for (const auto& pk : valuesFound)
      {
        // lookup router
        if (router and router->nodedb()->Has(pk))
          continue;
        parent->LookupRouter(
            pk, std::bind(&AbstractRouter::HandleDHTLookupForExplore, router, pk, _1));
      }
    }
  }  // namespace dht
}  // namespace llarp
