#ifndef LLARP_DHT_EXPLORENETWORKJOB
#define LLARP_DHT_EXPLORENETWORKJOB

#include <dht/tx.hpp>
#include <router_id.hpp>

namespace llarp
{
  namespace dht
  {
    struct ExploreNetworkJob : public TX< RouterID, RouterID >
    {
      ExploreNetworkJob(const RouterID &peer, AbstractContext *ctx)
          : TX< RouterID, RouterID >(TXOwner{}, peer, ctx)
      {
      }

      bool
      Validate(const RouterID &) const override
      {
        // TODO: check with lokid
        return true;
      }

      void
      Start(const TXOwner &peer) override;

      bool
      GetNextPeer(Key_t &, const std::set< Key_t > &) override
      {
        return false;
      }

      void
      ExtractStatus(util::StatusObject &obj) const override
      {
        std::vector< std::string > foundObjs;
        for(const auto &found : valuesFound)
        {
          foundObjs.emplace_back(found.ToHex());
        }
        obj.PutStringArray("found", foundObjs);

        util::StatusObject txownerObj;
        txownerObj.PutInt("txid", whoasked.txid);
        txownerObj.PutString("node", whoasked.node.ToHex());
        obj.PutObject("whoasked", txownerObj);

        std::vector< std::string > asked;
        for(const auto &peer : peersAsked)
          asked.emplace_back(peer.ToHex());
        obj.PutStringArray("asked", asked);

        obj.PutString("target", target.ToHex());
      }

      void
      DoNextRequest(const Key_t &) override
      {
      }

      void
      SendReply() override;
    };
  }  // namespace dht
}  // namespace llarp

#endif
