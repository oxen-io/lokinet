#include "explorenetworkjob.hpp"

#include "context.hpp"
#include <llarp/dht/messages/findrouter.hpp>
#include <llarp/router/router.hpp>

#include <llarp/nodedb.hpp>

#include <llarp/tooling/dht_event.hpp>

namespace llarp
{
  namespace dht
  {
    void
    ExploreNetworkJob::Start(const TXOwner& peer)
    {
      auto msg = new FindRouterMessage(peer.txid);
      auto router = parent->GetRouter();
      if (router)
      {
        router->notify_router_event<tooling::FindRouterSentEvent>(router->pubkey(), *msg);
      }
      parent->DHTSendTo(peer.node.as_array(), msg);
    }

    void
    ExploreNetworkJob::SendReply()
    {
      llarp::LogDebug("got ", valuesFound.size(), " routers from exploration");

      auto router = parent->GetRouter();
      for (const auto& pk : valuesFound)
      {
        // lookup router
        if (router and router->node_db()->Has(pk))
          continue;
        parent->LookupRouter(
            pk, [router, pk](const auto& res) { router->HandleDHTLookupForExplore(pk, res); });
      }
    }
  }  // namespace dht
}  // namespace llarp
