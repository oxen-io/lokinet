#pragma once

#include <llarp/routing/dht_message.hpp>
#include "intro_set.hpp"
#include "lookup.hpp"

namespace llarp
{
  namespace service
  {
    struct Endpoint;
    struct HiddenServiceAddressLookup : public IServiceLookup
    {
      const PubKey rootkey;
      uint64_t relayOrder;
      const dht::Key_t location;
      using HandlerFunc = std::function<bool(
          const Address&, std::optional<IntroSet>, const RouterID&, llarp_time_t)>;
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

      bool
      HandleIntrosetResponse(const std::set<EncryptedIntroSet>& results) override;

      std::shared_ptr<routing::IMessage>
      BuildRequestMessage() override;
    };
  }  // namespace service
}  // namespace llarp
