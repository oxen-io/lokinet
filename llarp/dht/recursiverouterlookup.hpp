#ifndef LLARP_DHT_RECURSIVEROUTERLOOKUP
#define LLARP_DHT_RECURSIVEROUTERLOOKUP

#include <dht/tx.hpp>

#include <router_contact.hpp>
#include <router_id.hpp>

namespace llarp
{
  namespace dht
  {
    struct RecursiveRouterLookup : public TX< RouterID, RouterContact >
    {
      RouterLookupHandler resultHandler;
      RecursiveRouterLookup(const TXOwner &whoasked, const RouterID &target,
                            AbstractContext *ctx, RouterLookupHandler result);

      bool
      Validate(const RouterContact &rc) const override;

      bool
      GetNextPeer(Key_t &, const std::set< Key_t > &) override
      {
        return false;
      }

      void
      ExtractStatus(util::StatusObject &obj) const override
      {
        std::vector< util::StatusObject > foundObjs(valuesFound.size());
        {
          size_t idx = 0;
          for(const auto &found : valuesFound)
          {
            util::StatusObject &foundObj = foundObjs[idx++];
            found.ExtractStatus(foundObj);
          }
        }
        obj.PutObjectArray("found", foundObjs);

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
      Start(const TXOwner &peer) override;

      virtual void
      SendReply() override;
    };
  }  // namespace dht
}  // namespace llarp

#endif
