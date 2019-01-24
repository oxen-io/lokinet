#include <dht/explorenetworkjob.hpp>

#include <dht/context.hpp>
#include <dht/messages/findrouter.hpp>
#include <router/router.hpp>

namespace llarp
{
  namespace dht
  {
    void
    ExploreNetworkJob::Start(const TXOwner &peer)
    {
      parent->DHTSendTo(peer.node.as_array(), new FindRouterMessage(peer.txid));
    }

    void
    ExploreNetworkJob::SendReply()
    {
      llarp::LogInfo("got ", valuesFound.size(), " routers from exploration");
      for(const auto &pk : valuesFound)
      {
        // lookup router
        parent->LookupRouter(
            pk,
            std::bind(&Router::HandleDHTLookupForExplore, parent->GetRouter(), pk,
                      std::placeholders::_1));
      }
    }
  }  // namespace dht
}  // namespace llarp
