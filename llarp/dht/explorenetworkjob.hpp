#ifndef LLARP_DHT_EXPLORENETWORKJOB
#define LLARP_DHT_EXPLORENETWORKJOB

#include "tx.hpp"
#include <llarp/router_id.hpp>

namespace llarp::dht
{
  struct ExploreNetworkJob : public TX<RouterID, RouterID>
  {
    ExploreNetworkJob(const RouterID& peer, AbstractDHTMessageHandler* ctx)
        : TX<RouterID, RouterID>(TXOwner{}, peer, ctx)
    {}

    bool
    Validate(const RouterID&) const override
    {
      // TODO: check with lokid
      return true;
    }

    void
    Start(const TXOwner& peer) override;

    void
    SendReply() override;
  };
}  // namespace llarp::dht

#endif
