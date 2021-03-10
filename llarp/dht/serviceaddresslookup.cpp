#include "serviceaddresslookup.hpp"

#include "context.hpp"
#include <llarp/dht/messages/findintro.hpp>
#include <llarp/dht/messages/gotintro.hpp>
#include <utility>

namespace llarp
{
  namespace dht
  {
    ServiceAddressLookup::ServiceAddressLookup(
        const TXOwner& asker,
        const Key_t& addr,
        AbstractContext* ctx,
        uint32_t order,
        service::EncryptedIntroSetLookupHandler handler)
        : TX<TXOwner, service::EncryptedIntroSet>(asker, asker, ctx)
        , location(addr)
        , handleResult(std::move(handler))
        , relayOrder(order)
    {
      peersAsked.insert(ctx->OurKey());
    }

    bool
    ServiceAddressLookup::Validate(const service::EncryptedIntroSet& value) const
    {
      if (!value.Verify(parent->Now()))
      {
        llarp::LogWarn("Got invalid introset from service lookup");
        return false;
      }
      if (value.derivedSigningKey != location)
      {
        llarp::LogWarn("got introset with wrong target from service lookup");
        return false;
      }
      return true;
    }

    void
    ServiceAddressLookup::Start(const TXOwner& peer)
    {
      parent->DHTSendTo(
          peer.node.as_array(), new FindIntroMessage(peer.txid, location, relayOrder));
    }

    void
    ServiceAddressLookup::SendReply()
    {
      // get newest introset
      if (valuesFound.size())
      {
        llarp::service::EncryptedIntroSet found;
        for (const auto& introset : valuesFound)
        {
          if (found.OtherIsNewer(introset))
            found = introset;
        }
        valuesFound.clear();
        valuesFound.emplace_back(found);
      }
      if (handleResult)
      {
        handleResult(valuesFound);
      }
      parent->DHTSendTo(whoasked.node.as_array(), new GotIntroMessage(valuesFound, whoasked.txid));
    }
  }  // namespace dht
}  // namespace llarp
