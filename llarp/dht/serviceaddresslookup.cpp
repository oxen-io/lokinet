#include <dht/serviceaddresslookup.hpp>

#include <dht/context.hpp>
#include <dht/messages/findintro.hpp>
#include <dht/messages/gotintro.hpp>
#include <utility>

namespace llarp
{
  namespace dht
  {
    ServiceAddressLookup::ServiceAddressLookup(
        const TXOwner &asker, const Key_t &addr, AbstractContext *ctx,
        uint64_t r, service::EncryptedIntroSetLookupHandler handler)
        : TX< Key_t, service::EncryptedIntroSet >(asker, addr, ctx)
        , handleResult(std::move(handler))
        , R(r)
    {
      peersAsked.insert(ctx->OurKey());
    }

    bool
    ServiceAddressLookup::Validate(
        const service::EncryptedIntroSet &value) const
    {
      if(!value.Verify(parent->Now()))
      {
        llarp::LogWarn("Got invalid introset from service lookup");
        return false;
      }
      if(value.derivedSigningKey != target)
      {
        llarp::LogWarn("got introset with wrong target from service lookup");
        return false;
      }
      return true;
    }

    bool
    ServiceAddressLookup::GetNextPeer(Key_t &next,
                                      const std::set< Key_t > &exclude)
    {
      const auto &nodes = parent->Nodes();
      if(nodes)
      {
        return nodes->FindCloseExcluding(target, next, exclude);
      }

      return false;
    }

    void
    ServiceAddressLookup::Start(const TXOwner &peer)
    {
      parent->DHTSendTo(peer.node.as_array(),
                        new FindIntroMessage(peer.txid, target, R));
    }

    void
    ServiceAddressLookup::DoNextRequest(const Key_t &ask)
    {
      if(R)
      {
        parent->LookupIntroSetRecursive(target, whoasked.node, whoasked.txid,
                                        ask, R - 1);
      }
      else
      {
        parent->LookupIntroSetIterative(target, whoasked.node, whoasked.txid,
                                        ask);
      }
    }

    void
    ServiceAddressLookup::SendReply()
    {
      // get newest introset
      if(valuesFound.size())
      {
        llarp::service::EncryptedIntroSet found;
        for(const auto &introset : valuesFound)
        {
          if(found.OtherIsNewer(introset))
            found = introset;
        }
        valuesFound.clear();
        valuesFound.emplace_back(found);
      }
      if(handleResult)
      {
        handleResult(valuesFound);
      }
      parent->DHTSendTo(whoasked.node.as_array(),
                        new GotIntroMessage(valuesFound, whoasked.txid));
    }
  }  // namespace dht
}  // namespace llarp
