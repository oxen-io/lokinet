#ifndef LLARP_SERVICE_HIDDEN_SERVICE_ADDRESS_LOOKUP_HPP
#define LLARP_SERVICE_HIDDEN_SERVICE_ADDRESS_LOOKUP_HPP

#include <routing/dht_message.hpp>
#include <service/intro_set.hpp>
#include <service/lookup.hpp>

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
      using HandlerFunc =
          std::function<bool(const Address&, std::optional<IntroSet>, const RouterID&)>;
      HandlerFunc handle;

      HiddenServiceAddressLookup(
          Endpoint* p,
          HandlerFunc h,
          const dht::Key_t& location,
          const PubKey& rootkey,
          uint64_t relayOrder,
          uint64_t tx);

      ~HiddenServiceAddressLookup() override = default;

      bool
      HandleIntrosetResponse(const std::set<EncryptedIntroSet>& results) override;

      std::shared_ptr<routing::IMessage>
      BuildRequestMessage() override;
    };
  }  // namespace service
}  // namespace llarp

#endif
