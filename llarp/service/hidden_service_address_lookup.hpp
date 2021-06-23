#pragma once

#include <llarp/routing/dht_message.hpp>
#include "intro_set.hpp"
#include "lookup.hpp"

namespace llarp
{
  namespace service
  {
    /// interval for which we will add to lookup timeout interval
    constexpr auto IntrosetLookupGraceInterval = 20s;

    struct Endpoint;
    struct HiddenServiceAddressLookup : public IServiceLookup
    {
      const PubKey rootkey;
      uint64_t relayOrder;
      const dht::Key_t location;
      using HandlerFunc = std::function<bool(
          const Address&, std::optional<IntroSet>, const RouterID&, llarp_time_t, uint64_t)>;
      HandlerFunc handle;

      HiddenServiceAddressLookup(
          Endpoint* p,
          HandlerFunc h,
          const dht::Key_t& location,
          const PubKey& rootkey,
          const RouterID& routerAsked,
          uint64_t relayOrder,
          uint64_t tx,
          llarp_time_t timeout);

      ~HiddenServiceAddressLookup() override = default;

      virtual bool
      IsFor(EndpointBase::AddressVariant_t addr) const override
      {
        if (const auto* ptr = std::get_if<Address>(&addr))
        {
          return Address{rootkey} == *ptr;
        }
        return false;
      }

      bool
      HandleIntrosetResponse(const std::set<EncryptedIntroSet>& results) override;

      std::shared_ptr<routing::IMessage>
      BuildRequestMessage() override;
    };
  }  // namespace service
}  // namespace llarp
