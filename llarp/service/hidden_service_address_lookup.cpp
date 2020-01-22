#include <service/hidden_service_address_lookup.hpp>

#include <dht/messages/findintro.hpp>
#include <service/endpoint.hpp>
#include <utility>

namespace llarp
{
  namespace service
  {
    HiddenServiceAddressLookup::HiddenServiceAddressLookup(Endpoint* p,
                                                           HandlerFunc h,
                                                           const Address& addr,
                                                           uint64_t tx)
        : IServiceLookup(p, tx, "HSLookup"), remote(addr), handle(std::move(h))
    {
    }

    bool
    HiddenServiceAddressLookup::HandleResponse(
        const std::set< IntroSet >& results)
    {
      LogInfo("found ", results.size(), " for ", remote.ToString());
      if(results.size() > 0)
      {
        IntroSet selected;
        for(const auto& introset : results)
        {
          if(selected.OtherIsNewer(introset) && introset.A.Addr() == remote)
            selected = introset;
        }
        return handle(remote, &selected, endpoint);
      }
      return handle(remote, nullptr, endpoint);
    }

    std::shared_ptr< routing::IMessage >
    HiddenServiceAddressLookup::BuildRequestMessage()
    {
      auto msg = std::make_shared< routing::DHTMessage >();
      msg->M.emplace_back(
          std::make_unique< dht::FindIntroMessage >(txid, remote, false));
      return msg;
    }

  }  // namespace service
}  // namespace llarp
