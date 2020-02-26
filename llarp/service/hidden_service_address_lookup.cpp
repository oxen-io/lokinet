#include <service/hidden_service_address_lookup.hpp>

#include <dht/messages/findintro.hpp>
#include <service/endpoint.hpp>
#include <utility>

static constexpr size_t NUM_LOOKUP_REQUESTS_PER_SEND = 2;

namespace llarp
{
  namespace service
  {
    HiddenServiceAddressLookup::HiddenServiceAddressLookup(
        Endpoint* p, HandlerFunc h, const dht::Key_t& l, const PubKey& k,
        uint64_t order, uint64_t tx)
        : IServiceLookup(p, tx, "HSLookup", NUM_LOOKUP_REQUESTS_PER_SEND)
        , rootkey(k)
        , relayOrder(order)
        , location(l)
        , handle(std::move(h))
    {
    }

    bool
    HiddenServiceAddressLookup::OnHandleResponse(
        const std::set< EncryptedIntroSet >& results)
    {
      nonstd::optional< IntroSet > found;
      const Address remote(rootkey);
      LogInfo("found ", results.size(), " for ", remote.ToString());
      if(results.size() > 0)
      {
        EncryptedIntroSet selected;
        for(const auto& introset : results)
        {
          if(selected.OtherIsNewer(introset))
            selected = introset;
        }
        const auto maybe = selected.MaybeDecrypt(rootkey);
        if(maybe.has_value())
          found = maybe.value();
      }
      handle(remote, found, endpoint);
      return true;
    }

    void
    HiddenServiceAddressLookup::OnAllResponsesReceived()
    {
      const Address remote(rootkey);
      LogInfo("received all expected responses for lookup of ", remote.ToString());
    }

    std::shared_ptr< routing::IMessage >
    HiddenServiceAddressLookup::BuildRequestMessage()
    {
      auto msg = std::make_shared< routing::DHTMessage >();
      msg->M.emplace_back(std::make_unique< dht::FindIntroMessage >(
          txid, location, relayOrder));
      return msg;
    }

  }  // namespace service
}  // namespace llarp
