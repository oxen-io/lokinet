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
      Address remote;
      using HandlerFunc = std::function< bool(const Address&, const IntroSet*,
                                              const RouterID&) >;
      HandlerFunc handle;

      HiddenServiceAddressLookup(Endpoint* p, HandlerFunc h,
                                 const Address& addr, uint64_t tx);

      ~HiddenServiceAddressLookup() override = default;

      bool
      HandleResponse(const std::set< IntroSet >& results) override;

      std::shared_ptr< routing::IMessage >
      BuildRequestMessage() override;
    };
  }  // namespace service
}  // namespace llarp

#endif
